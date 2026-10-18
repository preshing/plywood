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
    EventHandler* eventHandler = nullptr;
    bool discardReturnValue = false;
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
    ReadNumber<double> result = readDoubleFromText(in);
    *value = result.value;
    return result.success && (in.curByte == in.endByte);
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
    EventHandler* handler = parser->eventHandler;
    if (handler) {
        handler->beginObject();
    }
    PLY_ON_SCOPE_EXIT({
        if (handler) {
            handler->endObject();
        }
    });
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
                    if (handler) {
                        handler->endObject();
                        handler = nullptr;
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
        const Node& existingNode = parser->discardReturnValue && parser->options.duplicateKeysOverrideEarlier
                                       ? Node::InvalidNode
                                       : node.get(propertyName);
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
            // Discarded duplicate values are still parsed and validated, without emitting events.
            if (isDuplicate && !parser->options.duplicateKeysOverrideEarlier) {
                parser->eventHandler = nullptr;
            }
            PLY_ON_SCOPE_EXIT({ parser->eventHandler = handler; });
            EventHandler* propertyHandler = parser->eventHandler;
            if (propertyHandler) {
                propertyHandler->beginProperty(propertyName);
            }
            PLY_ON_SCOPE_EXIT({
                if (propertyHandler) {
                    propertyHandler->endProperty();
                }
            });
            Node value = readExpression(parser, readToken(parser), &colon);
            if (!value.isValid())
                return value;
            if (!isDuplicate || parser->options.duplicateKeysOverrideEarlier) {
                // Either insert the property or override the earlier duplicate.
                if (!parser->discardReturnValue) {
                    node.set(propertyName, std::move(value));
                } else if (!parser->options.duplicateKeysOverrideEarlier) {
                    // Retain only the name and offset for duplicate detection.
                    node.set(propertyName, Node{Node::Null{}, value.fileOfs});
                }
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
    EventHandler* handler = parser->eventHandler;
    if (handler) {
        handler->beginArray();
    }
    PLY_ON_SCOPE_EXIT({
        if (handler) {
            handler->endArray();
        }
    });
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
                if (handler) {
                    handler->endArray();
                    handler = nullptr;
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
                if (!parser->discardReturnValue)
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
    // Read containers recursively and construct primitive values for event dispatch.
    Node node;
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
            node = Node{Node::Text{std::move(text)}, firstToken.inputOffset};
            break;
        }

        case Token::Identifier:
        case Token::NumericLiteral: {
            if (firstToken.text == "true" || firstToken.text == "false") {
                node = Node{Node::Bool{firstToken.text == "true"}, firstToken.inputOffset};
            } else if (firstToken.text == "null") {
                node = Node{Node::Null{}, firstToken.inputOffset};
            } else {
                double value = 0;
                if (tryParseNumber(firstToken.text, &value)) {
                    node = Node{Node::Number{value}, firstToken.inputOffset};
                } else {
                    // An unquoted word that isn't a JSON primitive is governed by the unquotedStrings policy.
                    if (!checkPolicy(parser, parser->options.unquotedStrings, firstToken.inputOffset,
                                     String::format("Unquoted string \"{}\"", escape(firstToken.text))))
                        return {};
                    node = Node{Node::Text{String{firstToken.text}}, firstToken.inputOffset};
                }
            }
            break;
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

    // Containers emit their own events; dispatch completed primitive values here.
    EventHandler* handler = parser->eventHandler;
    if (handler && node.isValid()) {
        if (const auto* text = node.var.as<Node::Text>()) {
            handler->string(text->text);
        } else if (const auto* boolean = node.var.as<Node::Bool>()) {
            handler->bool_(boolean->value);
        } else if (const auto* number = node.var.as<Node::Number>()) {
            handler->number(number->value);
        } else if (node.isNull()) {
            handler->null_();
        }
    }
    return node;
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

void Parser::setEventHandler(EventHandler* handler) {
    static_cast<ParserImpl*>(this)->eventHandler = handler;
}

void Parser::discardReturnValue() {
    static_cast<ParserImpl*>(this)->discardReturnValue = true;
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
            return {parser->discardReturnValue ? Node{} : std::move(root), std::move(parser->tokenLocMap), rootEnd};
        }
        return {parser->discardReturnValue ? Node{} : std::move(root), std::move(parser->tokenLocMap), parser->readOfs};
    }

    Token nextToken = readToken(parser);
    if (nextToken.type == Token::Invalid)
        return {{}, std::move(parser->tokenLocMap), parser->readOfs};
    if (nextToken.type != Token::EOF) {
        if (!checkPolicy(parser, parser->options.trailingInput, nextToken.inputOffset,
                         String::format("Unexpected {} after {}", describeToken(nextToken), describeNode(root))))
            return {{}, std::move(parser->tokenLocMap), parser->readOfs};
        return {parser->discardReturnValue ? Node{} : std::move(root), std::move(parser->tokenLocMap), rootEnd};
    }

    return {parser->discardReturnValue ? Node{} : std::move(root), std::move(parser->tokenLocMap), parser->readOfs};
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

// Serialize events incrementally, tracking separators and indentation for each container.
struct Writer : EventHandler {
    struct Scope {
        bool object;
        bool hasItems = false;
    };
    Stream& out;
    WriteOptions options;
    Array<Scope> scopes;

    Writer(Stream& out, const WriteOptions& options) : out{out}, options{options} {
    }

    // Write indentation for the current container depth.
    void indent() {
        if (this->options.includeWhitespace) {
            for (u32 i = 0; i < this->scopes.numItems(); i++) {
                this->out.write("  ");
            }
        }
    }

    // Separate successive children and indent the next one.
    void separator() {
        Scope& scope = this->scopes.back();
        if (scope.hasItems) {
            this->out.write(',');
            if (this->options.includeWhitespace) {
                this->out.write('\n');
            }
        }
        scope.hasItems = true;
        this->indent();
    }

    // Array values carry their own separators; object properties emit theirs.
    void beginValue() {
        if (!this->scopes.isEmpty() && !this->scopes.back().object) {
            this->separator();
        }
    }

    // Open a container and track its children.
    void beginContainer(bool object) {
        this->beginValue();
        this->out.write(object ? '{' : '[');
        if (this->options.includeWhitespace) {
            this->out.write('\n');
        }
        this->scopes.append({object});
    }

    // Close the container at its parent indentation level.
    void endContainer(bool object) {
        PLY_ASSERT(!this->scopes.isEmpty() && this->scopes.back().object == object);
        if (this->options.includeWhitespace && this->scopes.back().hasItems) {
            this->out.write('\n');
        }
        this->scopes.pop();
        this->indent();
        this->out.write(object ? '}' : ']');
    }

    void beginObject() override {
        this->beginContainer(true);
    }
    void endObject() override {
        this->endContainer(true);
    }
    void beginArray() override {
        this->beginContainer(false);
    }
    void endArray() override {
        this->endContainer(false);
    }
    void beginProperty(StringView name) override {
        PLY_ASSERT(!this->scopes.isEmpty() && this->scopes.back().object);
        this->separator();
        this->out.format("\"{}\":", escape(name));
        if (this->options.includeWhitespace) {
            this->out.write(' ');
        }
    }
    void string(StringView str) override {
        this->beginValue();
        this->out.format("\"{}\"", escape(str));
    }
    void bool_(bool value) override {
        this->beginValue();
        this->out.write(value ? "true" : "false");
    }
    void number(double value) override {
        this->beginValue();
        // Emit exactly representable integers without a fractional suffix for strict JSON schemas.
        if (isRepresentable<s64>(value)) {
            printNumber(this->out, (s64) value);
        } else {
            printNumber(this->out, value);
        }
    }
    void null_() override {
        this->beginValue();
        this->out.write("null");
    }
};

Owned<EventHandler> createWriter(Stream* out, const WriteOptions& options) {
    PLY_ASSERT(out);
    return Heap::create<Writer>(*out, options);
}

void write(EventHandler* handler, const Node& node) {
    PLY_ASSERT(handler);
    // Traverse containers in stored order and dispatch primitive values.
    if (const auto* obj = node.var.as<Node::Object>()) {
        handler->beginObject();
        for (const auto& item : obj->items.items()) {
            handler->beginProperty(item.key);
            write(handler, item.value);
            handler->endProperty();
        }
        handler->endObject();
    } else if (const auto* arr = node.var.as<Node::Array>()) {
        handler->beginArray();
        for (const Node& item : arr->items) {
            write(handler, item);
        }
        handler->endArray();
    } else if (const auto* number = node.var.as<Node::Number>()) {
        handler->number(number->value);
    } else if (const auto* boolean = node.var.as<Node::Bool>()) {
        handler->bool_(boolean->value);
    } else if (const auto* text = node.var.as<Node::Text>()) {
        handler->string(text->text);
    } else {
        handler->null_();
    }
}

void write(Stream& out, const Node& node, const WriteOptions& options) {
    Owned<EventHandler> handler = createWriter(&out, options);
    write(handler, node);
}

String toString(const Node& node, const WriteOptions& options) {
    MemStream out;
    write(out, node, options);
    return out.moveToString();
}

} // namespace json
} // namespace ply
