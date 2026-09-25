/*─────────────────────────────────────────────────────────────────┐
│                                                                  │
│     ____      Plywood C++ Runtime Library                        │
│    ╱   ╱╲     https://plywood.dev/                               │
│   ╱___╱╭╮╲                                                       │
│    └──┴┴┴┘    JSON Support                                       │
│               Documentation: docs/high-level/json-support.md     │
│                                                                  │
└─────────────────────────────────────────────────────────────────*/

#include "ply-json.h"

namespace ply {
namespace json {

//  ▄▄  ▄▄            ▄▄
//  ███ ██  ▄▄▄▄   ▄▄▄██  ▄▄▄▄
//  ██▀███ ██  ██ ██  ██ ██▄▄██
//  ██  ██ ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄▄
//

Node Node::InvalidNode;
Node::Object Node::EmptyObject;

Node& Node::get(StringView key) {
    Object* obj = this->var.as<Object>();
    if (!obj)
        return InvalidNode;

    Node* value = obj->items.find(key);
    if (!value)
        return InvalidNode;

    return *value;
}

void Node::set(StringView key, Node&& value) {
    Object* obj = this->var.as<Object>();
    if (!obj)
        return;

    *obj->items.insert(key).value = std::move(value);
}

void Node::remove(StringView key) {
    Object* obj = this->var.as<Object>();
    if (!obj)
        return;

    obj->items.erase(key);
}

//  ▄▄▄▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██▄▄██ ██  ▀▀
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ▀█▄▄▄  ██
//

//-----------------------------------------------------------
// Helpers
//-----------------------------------------------------------

// Property names may be quoted strings or permissive unquoted literals.
bool isTextToken(const Token& token) {
    return token.type == Token::Identifier || token.type == Token::NumericLiteral || token.type == Token::StringLiteral;
}

struct ParserImpl : Parser {
    Options options;
    Functor<void(const Diagnostic& diagnostic)> diagnosticCallback;
    TokenLocationMap tokenLocMap;
    bool anyError_ = false;
    u32 readOfs = 0;
    Tokenizer tokenizer;
    ViewStream input;
    u32 tokenizerErrorOfs = 0;
    String tokenizerError;
    Array<Diagnostic::Scope> context;
};

// Returns true only when the entire literal parses as a number.
bool tryParseNumber(StringView text, double* value) {
    ViewStream in{text};
    *value = readDoubleFromText(in);
    return !in.inputError && (in.curByte == in.endByte);
}

// Forwards a diagnostic to the callback, if one is installed, and records fatal errors.
void emitDiagnostic(ParserImpl* parser, Diagnostic::Level level, u32 fileOfs, String&& message) {
    if (parser->diagnosticCallback) {
        Diagnostic diagnostic{level, fileOfs, std::move(message), parser->context};
        parser->diagnosticCallback(diagnostic);
    }
    if (level == Diagnostic::Error)
        parser->anyError_ = true;
}

// Emits a fatal error. Parsing stops and an invalid Node is returned to the caller.
void error(ParserImpl* parser, u32 fileOfs, String&& message) {
    emitDiagnostic(parser, Diagnostic::Error, fileOfs, std::move(message));
}

// Applies a parsing policy to a condition detected at fileOfs. Returns true if parsing may continue.
bool checkPolicy(ParserImpl* parser, Parser::Options::Policy policy, u32 fileOfs, String&& message) {
    switch (policy) {
        case Parser::Options::Permissive:
            return true;
        case Parser::Options::WarnAndContinue:
            emitDiagnostic(parser, Diagnostic::Warning, fileOfs, std::move(message));
            return true;
        default:
            emitDiagnostic(parser, Diagnostic::Error, fileOfs, std::move(message));
            return false;
    }
}

// FIXME: Maybe turn this into a format string because it's common
String escape(StringView str) {
    MemStream out;
    printEscapedString(out, str);
    return out.moveToString();
}

String describeToken(const Token& token) {
    switch (token.type) {
        case Token::Identifier:
        case Token::NumericLiteral:
            return String::format("text \"{}\"", escape(token.text));
        case Token::StringLiteral:
            return String::format("string literal {}", token.text);
        case Token::Unrecognized:
            return String::format("junk \"{}\"", escape(token.text));
        case Token::EOF:
            return "end of file";
        case Token::Invalid:
            return "invalid token";
        default:
            return String::format("\"{}\"", getPunctuationString(token.type));
    }
}

String describeNode(const Node& node) {
    if (node.var.is<Node::Object>()) {
        return "object";
    } else if (node.var.is<Node::Array>()) {
        return "array";
    } else if (const Node::Number* n = node.var.as<Node::Number>()) {
        return String::format("number {}", n->value);
    } else if (const Node::Text* txt = node.var.as<Node::Text>()) {
        return String::format("text \"{}\"", escape(txt->text));
    } else if (const Node::Bool* b = node.var.as<Node::Bool>()) {
        return String::format("bool {}", b->value ? "true" : "false");
    } else if (node.var.is<Node::Null>()) {
        return "null";
    }
    PLY_ASSERT(0);
    return "???";
}

//-----------------------------------------------------------
// Internal functions
//-----------------------------------------------------------

u32 hexDigitValue(s32 c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    c |= 0x20;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 16;
}

// Reads exactly four hexadecimal digits from a JSON Unicode escape.
bool readUnicodeEscape(ViewStream& in, u32& codePoint) {
    codePoint = 0;
    for (u32 i = 0; i < 4; i++) {
        u32 digit = in.hasRemainingBytes() ? hexDigitValue((u8) *in.curByte) : 16;
        if (digit >= 16)
            return false;
        codePoint = (codePoint << 4) | digit;
        in.curByte++;
    }
    return true;
}

// Rejects malformed, overlong, surrogate and out-of-range UTF-8 decodings.
bool isValidUTF8Decode(u8 firstByte, const DecodeResult& decoded) {
    if (decoded.status != DecodeStatus::OK)
        return false;
    if (decoded.numBytes == 2)
        return firstByte >= 0xc2;
    if (decoded.numBytes == 3)
        return decoded.point >= 0x800 && !(decoded.point >= 0xd800 && decoded.point < 0xe000);
    if (decoded.numBytes == 4)
        return decoded.point >= 0x10000 && decoded.point <= 0x10ffff;
    return decoded.numBytes == 1;
}

// Decode a token spelling while enforcing JSON string policies at source offsets.
bool decodeString(ParserImpl* parser, const Token& source, String& text) {
    ViewStream in{source.text};
    auto offset = [&]() { return source.inputOffset + numericCast<u32>(in.curByte - source.text.bytes()); };
    auto peek = [&]() -> s32 { return in.hasRemainingBytes() ? (u8) *in.curByte : -1; };
    PLY_ON_SCOPE_EXIT({ parser->readOfs = offset(); });
    bool wasSingleQuoted = peek() == '\'';
    s32 quote = peek();
    if (wasSingleQuoted &&
        !checkPolicy(parser, parser->options.singleQuotedStrings, source.inputOffset, "Single-quoted string"))
        return false;
    in.curByte++;

    // Decode the string while applying each independently configurable extension.
    MemStream out;
    for (;;) {
        if (peek() < 0) {
            error(parser, offset(), "Unexpected end of file in string literal");
            return false;
        }
        if (peek() == quote) {
            in.curByte++;
            text = out.moveToString();
            return true;
        }

        u32 charOfs = offset();
        u8 c = (u8) peek();
        // Decode JSON escape sequences and diagnose any enabled nonstandard forms.
        if (c == '\\') {
            in.curByte++;
            if (peek() < 0) {
                error(parser, offset(), "Unexpected end of file in string literal");
                return false;
            }
            u8 escape = (u8) peek();
            in.curByte++;
            switch (escape) {
                case '"':
                case '\\':
                case '/':
                    out.write(escape);
                    break;
                case '\'':
                    if (quote == '\'') {
                        out.write(escape);
                        break;
                    }
                    if (!checkPolicy(parser, parser->options.arbitraryEscapeChars, charOfs,
                                     "Nonstandard escape sequence in string literal"))
                        return false;
                    out.write(escape);
                    break;
                case 'b':
                    out.write('\b');
                    break;
                case 'f':
                    out.write('\f');
                    break;
                case 'n':
                    out.write('\n');
                    break;
                case 'r':
                    out.write('\r');
                    break;
                case 't':
                    out.write('\t');
                    break;
                case 'u': {
                    u32 codePoint = 0;
                    if (!readUnicodeEscape(in, codePoint)) {
                        error(parser, offset(), "Bad Unicode escape sequence in string literal");
                        return false;
                    }
                    // Combine a valid UTF-16 surrogate pair into one Unicode code point.
                    if (codePoint >= 0xd800 && codePoint < 0xdc00) {
                        if (peek() != '\\' || in.numRemainingBytes() < 2 || in.curByte[1] != 'u') {
                            error(parser, offset(), "Unpaired Unicode surrogate in string literal");
                            return false;
                        }
                        in.curByte++;
                        in.curByte++;
                        u32 lowSurrogate = 0;
                        if (!readUnicodeEscape(in, lowSurrogate) || lowSurrogate < 0xdc00 || lowSurrogate >= 0xe000) {
                            error(parser, offset(), "Unpaired Unicode surrogate in string literal");
                            return false;
                        }
                        codePoint = 0x10000 + (((codePoint - 0xd800) << 10) | (lowSurrogate - 0xdc00));
                    } else if (codePoint >= 0xdc00 && codePoint < 0xe000) {
                        error(parser, offset(), "Unpaired Unicode surrogate in string literal");
                        return false;
                    }
                    encodeUnicode(out, UnicodeType::UTF8, codePoint);
                    break;
                }
                default:
                    if (!checkPolicy(parser, parser->options.arbitraryEscapeChars, charOfs,
                                     "Nonstandard escape sequence in string literal"))
                        return false;
                    out.write(escape);
                    break;
            }
            continue;
        }

        // Apply the configured policy to unescaped ASCII control characters.
        if (c < 0x20) {
            if (!checkPolicy(parser, parser->options.unescapedControlChars, charOfs,
                             "Unescaped control character in string literal"))
                return false;
            out.write(c);
            in.curByte++;
            continue;
        }

        // Validate each source-encoded character but preserve its original bytes.
        DecodeResult decoded = decodeUnicode(StringView{in.curByte, in.endByte}, UnicodeType::UTF8);
        if (!isValidUTF8Decode(c, decoded) &&
            !checkPolicy(parser, parser->options.nonUTF8Strings, charOfs, "Invalid UTF-8 in string literal"))
            return false;
        u32 numBytes = max(decoded.numBytes, u32(1));
        out.write(StringView{in.curByte, numBytes});
        in.curByte += numBytes;
    }
}

Token readToken(ParserImpl* parser) {
    // Skip whitespace and retain tokenizer diagnostics until the token's kind is known.
    parser->tokenizerError = {};
    Token source;
    do {
        source = ply::readToken(parser->tokenizer, parser->input);
    } while (source.type == Token::Whitespace);
    parser->readOfs = parser->tokenizer.inputOffset;

    // String consumers decode and diagnose their own literals, including incomplete ones.
    if (source.type != Token::StringLiteral && !parser->tokenizerError.isEmpty()) {
        error(parser, parser->tokenizerErrorOfs, std::move(parser->tokenizerError));
        return {};
    }
    return source;
}

Node readExpression(ParserImpl* parser, const Token& firstToken, const Token* afterToken);

struct ScopeHandler {
    ParserImpl& parser;
    u32 index;

    ScopeHandler(ParserImpl& parser, Diagnostic::Scope&& scope) : parser{parser}, index{parser.context.numItems()} {
        parser.context.append(std::move(scope));
    }
    ~ScopeHandler() {
        // parser.context can be empty when a fatal error is thrown
        if (!this->parser.context.isEmpty()) {
            PLY_ASSERT(this->parser.context.numItems() == this->index + 1);
            this->parser.context.pop();
        }
    }
    Diagnostic::Scope& get() {
        return this->parser.context[this->index];
    }
};

Node readObject(ParserImpl* parser, const Token& startToken) {
    PLY_ASSERT(startToken.type == Token::OpenCurly);
    ScopeHandler objectScope{*parser, Diagnostic::Scope::object(startToken.inputOffset)};
    Node node{Node::Object{}, startToken.inputOffset};
    String prevProperty;
    bool gotAnyProperty = false;
    for (;;) {
        // Consume separators until the next property or the end of the object is found.
        bool gotComma = false; // A comma or semicolon was seen after the previous property.
        Token firstToken = {};
        for (;;) {
            firstToken = readToken(parser);
            switch (firstToken.type) {
                case Token::CloseCurly: {
                    if (gotComma) {
                        // The object ends with a trailing separator.
                        if (!checkPolicy(parser, parser->options.looseSeparators, firstToken.inputOffset,
                                         "Separator not allowed before \"}\""))
                            return {};
                    }
                    return node;
                }

                case Token::Comma:
                case Token::Semicolon: {
                    if (firstToken.type == Token::Semicolon) {
                        // Semicolons are governed by the alternateSeparators policy.
                        if (!checkPolicy(parser, parser->options.alternateSeparators, firstToken.inputOffset,
                                         "Semicolons are not allowed as separators"))
                            return {};
                    }
                    if (gotComma || !gotAnyProperty) {
                        // The separator is redundant or appears before the first property.
                        if (!checkPolicy(parser, parser->options.looseSeparators, firstToken.inputOffset,
                                         "Misplaced separator in object"))
                            return {};
                    }
                    gotComma = true;
                    break;
                }

                default:
                    goto breakOuter;
            }
        }

    breakOuter:
        if (firstToken.type == Token::Invalid)
            return {};
        String propertyName;
        if (isTextToken(firstToken)) {
            // Keep the decoded name alive across recursive parsing and diagnostic scopes.
            if (firstToken.type == Token::StringLiteral) {
                if (!decodeString(parser, firstToken, propertyName))
                    return {};
            } else {
                propertyName = firstToken.text;
            }
            if (gotAnyProperty && !gotComma) {
                // No separator between two properties.
                if (!checkPolicy(parser, parser->options.looseSeparators, firstToken.inputOffset,
                                 String::format("Expected a separator between properties \"{}\" and \"{}\"",
                                                escape(prevProperty), escape(propertyName))))
                    return {};
            }
            if (firstToken.type != Token::StringLiteral) {
                // Unquoted property names are governed by the unquotedKeys policy.
                if (!checkPolicy(parser, parser->options.unquotedKeys, firstToken.inputOffset,
                                 String::format("Property name \"{}\" must be double-quoted", escape(propertyName))))
                    return {};
            }
        } else if (gotAnyProperty) {
            error(
                parser, firstToken.inputOffset,
                String::format("Unexpected {} after property \"{}\"", describeToken(firstToken), escape(prevProperty)));
            return {};
        } else {
            error(parser, firstToken.inputOffset,
                  String::format("Expected property, got {}", describeToken(firstToken)));
            return {};
        }

        // Check for a duplicate property name.
        const Node& existingNode = node.get(propertyName);
        bool isDuplicate = existingNode.isValid();
        if (isDuplicate) {
            ScopeHandler duplicateScope{*parser, Diagnostic::Scope::duplicate(existingNode.fileOfs)};
            if (!checkPolicy(parser, parser->options.duplicateKeys, firstToken.inputOffset,
                             String::format("Duplicate property \"{}\"", escape(propertyName))))
                return {};
        }

        Token colon = readToken(parser);
        if (colon.type == Token::SingleEqual) {
            // Using '=' instead of ':' is governed by the equalsSign policy.
            if (!checkPolicy(parser, parser->options.equalsSign, colon.inputOffset,
                             String::format("Property \"{}\" uses \"=\" instead of \":\"", escape(propertyName))))
                return {};
        } else if (colon.type != Token::SingleColon) {
            error(parser, colon.inputOffset,
                  String::format("Expected \":\" after \"{}\", got {}", escape(propertyName), describeToken(colon)));
            return {};
        }

        {
            // Read the value of the property.
            ScopeHandler propertyScope{*parser, Diagnostic::Scope::property(firstToken.inputOffset, propertyName)};
            Node value = readExpression(parser, readToken(parser), &colon);
            if (!value.isValid())
                return value;
            if (!isDuplicate || parser->options.duplicateKeysOverrideEarlier) {
                // Either insert the property or override the earlier duplicate.
                node.set(propertyName, std::move(value));
            }
        }

        prevProperty = std::move(propertyName);
        gotAnyProperty = true;
    }
    return {};
}

Node readArray(ParserImpl* parser, const Token& startToken) {
    PLY_ASSERT(startToken.type == Token::OpenSquare);
    ScopeHandler arrayScope{*parser, Diagnostic::Scope::array(startToken.inputOffset, 0)};
    Node arrayNode{Node::Array{}, startToken.inputOffset};
    Token sepTokenHolder;
    const Token* sepToken = nullptr;
    bool gotAnyItem = false;
    bool gotComma = false; // A comma or semicolon was seen after the previous item.
    for (;;) {
        Token token = readToken(parser);
        switch (token.type) {
            case Token::CloseSquare: {
                if (gotComma) {
                    // The array ends with a trailing separator.
                    if (!checkPolicy(parser, parser->options.looseSeparators, token.inputOffset,
                                     "Separator not allowed before \"]\""))
                        return {};
                }
                return arrayNode;
            }

            case Token::Comma:
            case Token::Semicolon: {
                if (token.type == Token::Semicolon) {
                    // Semicolons are governed by the alternateSeparators policy.
                    if (!checkPolicy(parser, parser->options.alternateSeparators, token.inputOffset,
                                     "Semicolons are not allowed as separators"))
                        return {};
                }
                if (gotComma || !gotAnyItem) {
                    // The separator is redundant or appears before the first item.
                    if (!checkPolicy(parser, parser->options.looseSeparators, token.inputOffset,
                                     "Misplaced separator in array"))
                        return {};
                }
                gotComma = true;
                sepTokenHolder = token;
                sepToken = &sepTokenHolder;
                break;
            }

            default: {
                if (token.type == Token::Invalid)
                    return {};
                if (gotAnyItem && !gotComma) {
                    // No separator between two items.
                    if (!checkPolicy(parser, parser->options.looseSeparators, token.inputOffset,
                                     String::format("Expected a separator before {}", describeToken(token))))
                        return {};
                }
                Node value = readExpression(parser, token, sepToken);
                if (!value.isValid())
                    return value;
                arrayNode.array().append(std::move(value));
                arrayScope.get().index++;
                gotAnyItem = true;
                gotComma = false;
                sepToken = nullptr;
                break;
            }
        }
    }
}

Node readExpression(ParserImpl* parser, const Token& firstToken, const Token* afterToken = nullptr) {
    switch (firstToken.type) {
        case Token::OpenCurly:
            return readObject(parser, firstToken);

        case Token::OpenSquare:
            return readArray(parser, firstToken);

        case Token::StringLiteral: {
            // Decode directly into the value's owned string.
            String text;
            if (!decodeString(parser, firstToken, text))
                return {};
            return Node{Node::Text{std::move(text)}, firstToken.inputOffset};
        }

        case Token::Identifier:
        case Token::NumericLiteral: {
            if (firstToken.text == "true")
                return Node{Node::Bool{true}, firstToken.inputOffset};
            if (firstToken.text == "false")
                return Node{Node::Bool{false}, firstToken.inputOffset};
            if (firstToken.text == "null")
                return Node{Node::Null{}, firstToken.inputOffset};
            double value = 0;
            if (tryParseNumber(firstToken.text, &value))
                return Node{Node::Number{value}, firstToken.inputOffset};
            // An unquoted word that isn't a JSON primitive is governed by the unquotedStrings policy.
            if (!checkPolicy(parser, parser->options.unquotedStrings, firstToken.inputOffset,
                             String::format("Unquoted string \"{}\"", escape(firstToken.text))))
                return {};
            return Node{Node::Text{String{firstToken.text}}, firstToken.inputOffset};
        }

        case Token::Invalid:
            return {};

        default: {
            MemStream mout;
            mout.format("Unexpected {} after {}", describeToken(firstToken),
                        afterToken ? describeToken(*afterToken) : "the start of the input");
            error(parser, firstToken.inputOffset, mout.moveToString());
            return {};
        }
    }
}

//-----------------------------------------------------------
// Public API
//-----------------------------------------------------------

Parser::Options Parser::Options::makeStrict() {
    // In strict mode, every permissive extension of standard JSON is rejected.
    Options options;
    options.trailingInput = FatalError;
    options.unquotedKeys = FatalError;
    options.unquotedStrings = FatalError;
    options.equalsSign = FatalError;
    options.alternateSeparators = FatalError;
    options.looseSeparators = FatalError;
    options.singleQuotedStrings = FatalError;
    options.nonUTF8Strings = FatalError;
    options.unescapedControlChars = FatalError;
    options.arbitraryEscapeChars = FatalError;
    options.duplicateKeys = FatalError;
    return options;
}

Owned<Parser> Parser::create(const Options& options) {
    ParserImpl* parser = Heap::create<ParserImpl>();
    parser->options = options;
    return parser;
}

void Parser::destroy() {
    Heap::destroy(static_cast<ParserImpl*>(this));
}

void Parser::setDiagnosticCallback(Functor<void(const Diagnostic& diagnostic)>&& cb) {
    static_cast<ParserImpl*>(this)->diagnosticCallback = std::move(cb);
}

ParseResult Parser::parse(StringView path, StringView srcView) {
    ParserImpl* parser = static_cast<ParserImpl*>(this);
    parser->readOfs = 0;
    parser->tokenizer = {};
    parser->tokenizer.config = Tokenizer::Config::jsonMode();
    parser->input = ViewStream{srcView};
    parser->tokenizer.errorCallback = [parser](u32 ofs, String&& message) {
        parser->tokenizerErrorOfs = ofs;
        parser->tokenizerError = std::move(message);
    };

    parser->tokenLocMap = TokenLocationMap::createFromString(srcView, max(parser->options.tabSize, u32(1)));

    Token rootToken = readToken(parser);
    Node root = readExpression(parser, rootToken);
    if (!root.isValid())
        return {{}, std::move(parser->tokenLocMap), parser->readOfs};

    u32 rootEnd = parser->readOfs;
    if (parser->options.trailingInput != Options::FatalError) {
        // Locate trailing input without tokenizing it, since accepted trailing bytes are outside the parsed value.
        while (parser->input.hasRemainingBytes()) {
            char c = *parser->input.curByte;
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                break;
            parser->input.curByte++;
            parser->readOfs++;
        }
        if (parser->input.hasRemainingBytes()) {
            checkPolicy(parser, parser->options.trailingInput, parser->readOfs,
                        String::format("Unexpected trailing input after {}", describeNode(root)));
            return {std::move(root), std::move(parser->tokenLocMap), rootEnd};
        }
        return {std::move(root), std::move(parser->tokenLocMap), parser->readOfs};
    }

    Token nextToken = readToken(parser);
    if (nextToken.type == Token::Invalid)
        return {{}, std::move(parser->tokenLocMap), parser->readOfs};
    if (nextToken.type != Token::EOF) {
        if (!checkPolicy(parser, parser->options.trailingInput, nextToken.inputOffset,
                         String::format("Unexpected {} after {}", describeToken(nextToken), describeNode(root))))
            return {{}, std::move(parser->tokenLocMap), parser->readOfs};
        return {std::move(root), std::move(parser->tokenLocMap), rootEnd};
    }

    return {std::move(root), std::move(parser->tokenLocMap), parser->readOfs};
}

bool Parser::anyError() const {
    return static_cast<const ParserImpl*>(this)->anyError_;
}

void Parser::printDiagnostic(const Diagnostic& diagnostic, Stream& out) const {
    const ParserImpl* parser = static_cast<const ParserImpl*>(this);
    TokenLocation diagnosticLoc = parser->tokenLocMap.getLocationFromOffset(diagnostic.fileOfs);
    out.format("({}, {}): {}: {}\n", diagnosticLoc.lineNumber, diagnosticLoc.columnNumber,
               diagnostic.errorLevel == Diagnostic::Warning ? "warning" : "error", diagnostic.message);
    for (u32 i = 0; i < diagnostic.context.numItems(); i++) {
        const Diagnostic::Scope& scope = diagnostic.context.back(-(s32) i - 1);
        TokenLocation contextLoc = parser->tokenLocMap.getLocationFromOffset(scope.fileOfs);
        out.format("({}, {}) ", contextLoc.lineNumber, contextLoc.columnNumber);
        switch (scope.type) {
            case Diagnostic::Scope::Object:
                out.write("while reading object started here");
                break;

            case Diagnostic::Scope::Property:
                out.format("while reading property {} started here", scope.name);
                break;

            case Diagnostic::Scope::Duplicate:
                out.write("existing property was defined here");
                break;

            case Diagnostic::Scope::Array:
                out.format("while reading item {} of the array started here (index is zero-based)", scope.index);
                break;
        }
        out.write('\n');
    }
}

//  ▄▄    ▄▄        ▄▄  ▄▄
//  ██ ▄▄ ██ ▄▄▄▄▄  ▄▄ ▄██▄▄  ▄▄▄▄
//  ▀█▄██▄█▀ ██  ▀▀ ██  ██   ██▄▄██
//   ██▀▀██  ██     ██  ▀█▄▄ ▀█▄▄▄
//

struct WriteContext {
    Stream& out;
    WriteOptions options;
    u32 indentLevel = 0;

    WriteContext(Stream& out, const WriteOptions& options) : out{out}, options{options} {
    }

    void indent() {
        if (this->options.includeWhitespace) {
            for (u32 i = 0; i < this->indentLevel; i++) {
                this->out.write("  ");
            }
        }
    }

    void write(const Node& node) {
        if (!node.isValid()) {
            this->out.write("null");
            return;
        }

        if (const Node::Object* obj = node.var.as<Node::Object>()) {
            this->out.write('{');
            if (this->options.includeWhitespace) {
                this->out.write('\n');
            }
            this->indentLevel++;
            ArrayView<const Map<String, Node>::Item> items = obj->items.items();
            for (u32 itemIndex = 0; itemIndex < items.numItems(); itemIndex++) {
                const auto& objItem = items[itemIndex];
                indent();
                this->out.format("\"{}\":", escape(objItem.key));
                if (this->options.includeWhitespace) {
                    this->out.write(' ');
                }
                write(objItem.value);
                if (itemIndex < items.numItems() - 1) {
                    this->out.write(',');
                }
                if (this->options.includeWhitespace) {
                    this->out.write('\n');
                }
            }
            this->indentLevel--;
            indent();
            this->out.write('}');
        } else if (const Node::Array* arr = node.var.as<Node::Array>()) {
            this->out.write('[');
            if (this->options.includeWhitespace) {
                this->out.write('\n');
            }
            this->indentLevel++;
            for (u32 i = 0; i < arr->items.numItems(); i++) {
                indent();
                write(arr->items[i]);
                if (i < arr->items.numItems() - 1) {
                    this->out.write(',');
                }
                if (this->options.includeWhitespace) {
                    this->out.write('\n');
                }
            }
            this->indentLevel--;
            indent();
            this->out.write(']');
        } else if (const Node::Number* n = node.var.as<Node::Number>()) {
            // Emit exactly representable integers without a fractional suffix for strict JSON schemas.
            if (isRepresentable<s64>(n->value)) {
                printNumber(this->out, (s64) n->value);
            } else {
                printNumber(this->out, n->value);
            }
        } else if (const Node::Bool* b = node.var.as<Node::Bool>()) {
            this->out.write(b->value ? "true" : "false");
        } else if (const Node::Text* txt = node.var.as<Node::Text>()) {
            this->out.format("\"{}\"", escape(txt->text));
        } else {
            this->out.write("null");
        }
    }
};

void write(Stream& out, const Node& node, const WriteOptions& options) {
    WriteContext ctx{out, options};
    ctx.write(node);
}

String toString(const Node& node, const WriteOptions& options) {
    MemStream out;
    write(out, node, options);
    return out.moveToString();
}

} // namespace json
} // namespace ply
