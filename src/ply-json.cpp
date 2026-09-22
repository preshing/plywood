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

bool isAlnumUnit(u32 c) {
    return (c == '_') || (c == '$') || (c == '-') || (c == '.') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || (c >= 128);
}

struct Token {
    enum Type {
        Invalid,
        OpenCurly,
        CloseCurly,
        OpenSquare,
        CloseSquare,
        Colon,
        Equals,
        Comma,
        Semicolon,
        Text,
        Junk,
        NewLine,
        EndOfFile,
    };
    Type type = Invalid;
    u32 fileOfs = 0;
    String text;
    bool wasQuoted = false;

    bool isValid() const {
        return type != Type::Invalid;
    }
};

struct ParserImpl : Parser {
    Functor<void(const ParseError& err)> errorCallback;
    TokenLocationMap tokenLocMap;
    bool anyError_ = false;
    // The parser expects only a single JSON expression as input.
    // If greedy is true and there are more tokens after the initial expression, it logs an error.
    bool greedy = true;
    StringView srcView;
    u32 readOfs = 0;
    s32 nextUnit = 0;
    u32 tabSize = 4;
    Token pushBackToken;
    Array<ParseError::Scope> context;

    void pushBack(Token&& token) {
        pushBackToken = std::move(token);
    }
};

// Returns true only when the entire literal parses as a number.
bool tryParseNumber(StringView text, double* value) {
    ViewStream in{text};
    *value = readDoubleFromText(in);
    return !in.inputError && (in.curByte == in.endByte);
}

void error(ParserImpl* parser, u32 fileOfs, String&& message) {
    if (parser->errorCallback) {
        ParseError err{fileOfs, std::move(message), parser->context};
        parser->errorCallback(err);
    }
    parser->anyError_ = true;
}

void advanceChar(ParserImpl* parser) {
    if (parser->readOfs + 1 < parser->srcView.numBytes()) {
        parser->readOfs++;
        parser->nextUnit = parser->srcView.bytes()[parser->readOfs];
    } else {
        parser->nextUnit = -1;
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
        case Token::OpenCurly:
            return "\"{\"";
        case Token::CloseCurly:
            return "\"}\"";
        case Token::OpenSquare:
            return "\"[\"";
        case Token::CloseSquare:
            return "\"]\"";
        case Token::Colon:
            return "\":\"";
        case Token::Equals:
            return "\"=\"";
        case Token::Comma:
            return "\",\"";
        case Token::Semicolon:
            return "\";\"";
        case Token::Text:
            return String::format("text \"{}\"", escape(token.text));
        case Token::Junk:
            return String::format("junk \"{}\"", escape(token.text));
        case Token::NewLine:
            return "newline";
        case Token::EndOfFile:
            return "end of file";
        default:
            PLY_ASSERT(0);
            return "???";
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
    }
    PLY_ASSERT(0);
    return "???";
}

//-----------------------------------------------------------
// Internal functions
//-----------------------------------------------------------

Token readPlainToken(ParserImpl* parser, Token::Type type) {
    Token result = {type, parser->readOfs, {}};
    advanceChar(parser);
    return result;
}

Token readLiteral(ParserImpl* parser) {
    PLY_ASSERT(isAlnumUnit(parser->nextUnit));

    if (parser->nextUnit == '-' || (parser->nextUnit >= '0' && parser->nextUnit <= '9')) {
        Token token = {Token::Text, parser->readOfs, {}};
        u32 startOfs = parser->readOfs;

        if (parser->nextUnit == '-') {
            advanceChar(parser);
        }

        if (parser->nextUnit == '0') {
            advanceChar(parser);
        } else {
            while (parser->nextUnit >= '0' && parser->nextUnit <= '9') {
                advanceChar(parser);
            }
        }

        if (parser->nextUnit == '.') {
            advanceChar(parser);
            while (parser->nextUnit >= '0' && parser->nextUnit <= '9') {
                advanceChar(parser);
            }
        }

        if ((parser->nextUnit | 0x20) == 'e') {
            advanceChar(parser);
            if (parser->nextUnit == '+' || parser->nextUnit == '-') {
                advanceChar(parser);
            }
            while (parser->nextUnit >= '0' && parser->nextUnit <= '9') {
                advanceChar(parser);
            }
        }

        token.text = StringView{(char*) parser->srcView.bytes() + startOfs, parser->readOfs - startOfs};
        return token;
    }

    Token token = {Token::Text, parser->readOfs, {}};
    u32 startOfs = parser->readOfs;

    while (isAlnumUnit(parser->nextUnit)) {
        advanceChar(parser);
    }

    token.text = StringView{(char*) parser->srcView.bytes() + startOfs, parser->readOfs - startOfs};
    return token;
}

Token readToken(ParserImpl* parser, bool tokenizeNewLine = false) {
    if (parser->pushBackToken.isValid()) {
        Token token = std::move(parser->pushBackToken);
        parser->pushBackToken = {};
        return token;
    }

    for (;;) {
        switch (parser->nextUnit) {
            case ' ':
            case '\t':
            case '\r':
                advanceChar(parser);
                break;

            case '\n': {
                u32 newLineOfs = parser->readOfs;
                advanceChar(parser);
                if (tokenizeNewLine)
                    return {Token::NewLine, newLineOfs, {}};
                break;
            }

            case -1:
                return {Token::EndOfFile, parser->readOfs, {}};
            case '{':
                return readPlainToken(parser, Token::OpenCurly);
            case '}':
                return readPlainToken(parser, Token::CloseCurly);
            case '[':
                return readPlainToken(parser, Token::OpenSquare);
            case ']':
                return readPlainToken(parser, Token::CloseSquare);
            case ':':
                return readPlainToken(parser, Token::Colon);
            case '=':
                return readPlainToken(parser, Token::Equals);
            case ',':
                return readPlainToken(parser, Token::Comma);
            case ';':
                return readPlainToken(parser, Token::Semicolon);

            case '"':
            case '\'': {
                Token token = {Token::Text, parser->readOfs, {}};
                token.wasQuoted = true;
                ViewStream in{parser->srcView.substr(parser->readOfs)};
                token.text =
                    readQuotedString(in, QuotedStringType::JSON, true, [parser, &in](QuotedStringError errorCode) {
                        u32 fileOfs = numericCast<u32>(in.curByte - parser->srcView.bytes());
                        switch (errorCode) {
                            case QuotedStringError::UnexpectedEndOfLine:
                                error(parser, fileOfs, "Unexpected end of line in string literal");
                                break;
                            case QuotedStringError::UnexpectedEndOfFile:
                                error(parser, fileOfs, "Unexpected end of file in string literal");
                                break;
                            case QuotedStringError::BadEscapeSequence:
                                error(parser, fileOfs, "Bad escape sequence in string literal");
                                break;
                            case QuotedStringError::NoOpeningQuote:
                                error(parser, fileOfs, "Expected opening quote in string literal");
                                break;
                        }
                    });
                parser->readOfs = numericCast<u32>(in.curByte - parser->srcView.bytes());
                parser->nextUnit =
                    (parser->readOfs < parser->srcView.numBytes()) ? parser->srcView[parser->readOfs] : -1;
                if (in.inputError)
                    return {};
                return token;
            }

            default:
                if (isAlnumUnit(parser->nextUnit))
                    return readLiteral(parser);
                else
                    return {Token::Junk, parser->readOfs, {}};
        }
    }
}

Node readExpression(ParserImpl* parser, Token&& firstToken, const Token* afterToken);

struct ScopeHandler {
    ParserImpl& parser;
    u32 index;

    ScopeHandler(ParserImpl& parser, ParseError::Scope&& scope) : parser{parser}, index{parser.context.numItems()} {
        parser.context.append(std::move(scope));
    }
    ~ScopeHandler() {
        // parser.context can be empty when Parse_Error is thrown
        if (!parser.context.isEmpty()) {
            PLY_ASSERT(parser.context.numItems() == index + 1);
            parser.context.pop();
        }
    }
    ParseError::Scope& get() {
        return parser.context[index];
    }
};

Node readObject(ParserImpl* parser, const Token& startToken) {
    PLY_ASSERT(startToken.type == Token::OpenCurly);
    ScopeHandler objectScope{*parser, ParseError::Scope::object(startToken.fileOfs)};
    Node node{Node::Object{}, startToken.fileOfs};
    Token prevProperty = {};
    for (;;) {
        bool gotSeparator = false;
        Token firstToken = {};
        for (;;) {
            firstToken = readToken(parser, true);
            switch (firstToken.type) {
                case Token::CloseCurly:
                    return node;

                case Token::Comma:
                case Token::Semicolon:
                case Token::NewLine:
                    gotSeparator = true;
                    break;

                default:
                    goto breakOuter;
            }
        }

    breakOuter:
        if (firstToken.type == Token::Text) {
            if (prevProperty.isValid() && !gotSeparator) {
                error(parser, firstToken.fileOfs,
                      String::format("Expected a comma, semicolon or newline "
                                     "separator between properties \"{}\" and \"{}\"",
                                     escape(prevProperty.text), escape(firstToken.text)));
                return {};
            }
        } else if (prevProperty.isValid()) {
            error(parser, firstToken.fileOfs,
                  String::format("Unexpected {} after property \"{}\"", describeToken(firstToken),
                                 escape(prevProperty.text)));
            return {};
        } else {
            error(parser, firstToken.fileOfs, String::format("Expected property, got {}", describeToken(firstToken)));
            return {};
        }

        const Node& existingNode = node.get(firstToken.text);
        if (existingNode.isValid()) {
            ScopeHandler duplicateScope{*parser, ParseError::Scope::duplicate(existingNode.fileOfs)};
            error(parser, firstToken.fileOfs, String::format("Duplicate property \"{}\"", escape(firstToken.text)));
            return {};
        }

        Token colon = readToken(parser);
        if (colon.type != Token::Colon && colon.type != Token::Equals) {
            error(parser, colon.fileOfs,
                  String::format("Expected \":\" or \"=\" after \"{}\", got {}", escape(firstToken.text),
                                 describeToken(colon)));
            return {};
        }

        {
            // Read value of property
            ScopeHandler propertyScope{*parser, ParseError::Scope::property(firstToken.fileOfs, firstToken.text)};
            Node value = readExpression(parser, readToken(parser), &colon);
            if (!value.isValid())
                return value;
            node.set(firstToken.text, std::move(value));
        }

        prevProperty = std::move(firstToken);
    }
    return {};
}

Node readArray(ParserImpl* parser, const Token& startToken) {
    PLY_ASSERT(startToken.type == Token::OpenSquare);
    ScopeHandler arrayScope{*parser, ParseError::Scope::array(startToken.fileOfs, 0)};
    Node arrayNode{Node::Array{}, startToken.fileOfs};
    Token sepTokenHolder;
    Token* sepToken = nullptr;
    for (;;) {
        Token token = readToken(parser, true);
        switch (token.type) {
            case Token::CloseSquare:
                return arrayNode;

            case Token::Comma:
            case Token::Semicolon:
            case Token::NewLine:
                sepTokenHolder = std::move(token);
                sepToken = &sepTokenHolder;
                break;

            default: {
                Node value = readExpression(parser, std::move(token), sepToken);
                if (!value.isValid())
                    return value;
                arrayNode.array().append(std::move(value));
                arrayScope.get().index++;
                sepToken = nullptr;
                break;
            }
        }
    }
}

Node readExpression(ParserImpl* parser, Token&& firstToken, const Token* afterToken = nullptr) {
    switch (firstToken.type) {
        case Token::OpenCurly:
            return readObject(parser, firstToken);

        case Token::OpenSquare:
            return readArray(parser, firstToken);

        case Token::Text: {
            // Quoted tokens are always JSON strings, even when their contents look like primitives.
            if (firstToken.wasQuoted) {
                return Node{Node::Text{std::move(firstToken.text)}, firstToken.fileOfs};
            }
            if (firstToken.text == "true") {
                return Node{Node::Bool{true}, firstToken.fileOfs};
            }
            if (firstToken.text == "false") {
                return Node{Node::Bool{false}, firstToken.fileOfs};
            }
            double value = 0;
            if (tryParseNumber(firstToken.text, &value)) {
                return Node{Node::Number{value}, firstToken.fileOfs};
            }
            return Node{Node::Text{std::move(firstToken.text)}, firstToken.fileOfs};
        }

        case Token::Invalid:
            return {};

        default: {
            MemStream mout;
            mout.format("Unexpected {} after {}", describeToken(firstToken),
                        afterToken ? describeToken(*afterToken) : "");
            error(parser, firstToken.fileOfs, mout.moveToString());
            return {};
        }
    }
}

//-----------------------------------------------------------
// Public API
//-----------------------------------------------------------

Owned<Parser> Parser::create() {
    return Heap::create<ParserImpl>();
}

void Parser::destroy() {
    Heap::destroy(static_cast<ParserImpl*>(this));
}

void Parser::setTabSize(int tabSize) {
    static_cast<ParserImpl*>(this)->tabSize = tabSize;
}

void Parser::setGreedy(bool greedy) {
    static_cast<ParserImpl*>(this)->greedy = greedy;
}

void Parser::setErrorCallback(Functor<void(const ParseError& err)>&& cb) {
    static_cast<ParserImpl*>(this)->errorCallback = std::move(cb);
}

ParseResult Parser::parse(StringView path, StringView srcView) {
    ParserImpl* parser = static_cast<ParserImpl*>(this);
    parser->srcView = srcView;
    parser->nextUnit = parser->srcView.numBytes() > 0 ? parser->srcView[0] : -1;

    parser->tokenLocMap = TokenLocationMap::createFromString(srcView);

    Token rootToken = readToken(parser);
    Node root = readExpression(parser, std::move(rootToken));
    if (!root.isValid())
        return {{}, std::move(parser->tokenLocMap), parser->readOfs};

    if (parser->greedy) {
        Token nextToken = readToken(parser);
        if (nextToken.type != Token::EndOfFile) {
            error(parser, nextToken.fileOfs,
                  String::format("Unexpected {} after {}", describeToken(nextToken), describeNode(root)));
            return {{}, std::move(parser->tokenLocMap), parser->readOfs};
        }
    }

    return {std::move(root), std::move(parser->tokenLocMap), parser->readOfs};
}

bool Parser::anyError() const {
    return static_cast<const ParserImpl*>(this)->anyError_;
}

void Parser::dumpError(const ParseError& error, Stream& out) const {
    const ParserImpl* parser = static_cast<const ParserImpl*>(this);
    TokenLocation errorLoc = parser->tokenLocMap.getLocationFromOffset(error.fileOfs);
    out.format("({}, {}): error: {}\n", errorLoc.lineNumber, errorLoc.columnNumber, error.message);
    for (u32 i = 0; i < error.context.numItems(); i++) {
        const ParseError::Scope& scope = error.context.back(-(s32) i - 1);
        TokenLocation contextLoc = parser->tokenLocMap.getLocationFromOffset(scope.fileOfs);
        out.format("({}, {}) ", contextLoc.lineNumber, contextLoc.columnNumber);
        switch (scope.type) {
            case ParseError::Scope::Object:
                out.write("while reading object started here");
                break;

            case ParseError::Scope::Property:
                out.format("while reading property {} started here", scope.name);
                break;

            case ParseError::Scope::Duplicate:
                out.write("existing property was defined here");
                break;

            case ParseError::Scope::Array:
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
