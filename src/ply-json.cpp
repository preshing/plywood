/*─────────────────────────────────────────────────────────────────┐
│                                                                  │
│     ____      Plywood C++ Runtime Library                        │
│    ╱   ╱╲     https://plywood.dev/                               │
│   ╱___╱╭╮╲                                                       │
│    └──┴┴┴┘    JSON Support                                       │
│               Documentation: /docs/high-level/json-support.md    │
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
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██▄▄██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ▀█▄▄▄
//

bool isAlnumUnit(u32 c) {
    return (c == '_') || (c == '$') || (c == '-') || (c == '.') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || (c >= 128);
}

// Returns true only when the entire literal parses as a number.
bool tryParseNumber(StringView text, double* value) {
    ViewStream in{text};
    *value = readDoubleFromText(in);
    return !in.inputError && (in.curByte == in.endByte);
}

void Parser::dumpError(const ParseError& error, Stream& out) const {
    TokenLocation errorLoc = this->tokenLocMap.getLocationFromOffset(error.fileOfs);
    out.format("({}, {}): error: {}\n", errorLoc.lineNumber, errorLoc.columnNumber, error.message);
    for (u32 i = 0; i < error.context.numItems(); i++) {
        const ParseError::Scope& scope = error.context.back(-(s32) i - 1);
        TokenLocation contextLoc = this->tokenLocMap.getLocationFromOffset(scope.fileOfs);
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

void Parser::error(u32 fileOfs, String&& message) {
    if (this->errorCallback) {
        ParseError err{fileOfs, std::move(message), context};
        this->errorCallback(err);
    }
    this->anyError_ = true;
}

void Parser::advanceChar() {
    if (this->readOfs + 1 < this->srcView.numBytes()) {
        this->readOfs++;
        this->nextUnit = this->srcView.bytes()[this->readOfs];
    } else {
        this->nextUnit = -1;
    }
}

Parser::Token Parser::readPlainToken(Token::Type type) {
    Token result = {type, this->readOfs, {}};
    this->advanceChar();
    return result;
}

Parser::Token Parser::readLiteral() {
    PLY_ASSERT(isAlnumUnit(this->nextUnit));

    if (this->nextUnit == '-' || (this->nextUnit >= '0' && this->nextUnit <= '9')) {
        Token token = {Token::Text, this->readOfs, {}};
        u32 startOfs = this->readOfs;

        if (this->nextUnit == '-') {
            this->advanceChar();
        }

        if (this->nextUnit == '0') {
            this->advanceChar();
        } else {
            while (this->nextUnit >= '0' && this->nextUnit <= '9') {
                this->advanceChar();
            }
        }

        if (this->nextUnit == '.') {
            this->advanceChar();
            while (this->nextUnit >= '0' && this->nextUnit <= '9') {
                this->advanceChar();
            }
        }

        if ((this->nextUnit | 0x20) == 'e') {
            this->advanceChar();
            if (this->nextUnit == '+' || this->nextUnit == '-') {
                this->advanceChar();
            }
            while (this->nextUnit >= '0' && this->nextUnit <= '9') {
                this->advanceChar();
            }
        }

        token.text = StringView{(char*) this->srcView.bytes() + startOfs, this->readOfs - startOfs};
        return token;
    }

    Token token = {Token::Text, this->readOfs, {}};
    u32 startOfs = this->readOfs;

    while (isAlnumUnit(this->nextUnit)) {
        this->advanceChar();
    }

    token.text = StringView{(char*) this->srcView.bytes() + startOfs, this->readOfs - startOfs};
    return token;
}

Parser::Token Parser::readToken(bool tokenizeNewLine) {
    if (this->pushBackToken.isValid()) {
        Token token = std::move(this->pushBackToken);
        this->pushBackToken = {};
        return token;
    }

    for (;;) {
        switch (this->nextUnit) {
            case ' ':
            case '\t':
            case '\r':
                this->advanceChar();
                break;

            case '\n': {
                u32 newLineOfs = this->readOfs;
                this->advanceChar();
                if (tokenizeNewLine)
                    return {Token::NewLine, newLineOfs, {}};
                break;
            }

            case -1:
                return {Token::EndOfFile, this->readOfs, {}};
            case '{':
                return this->readPlainToken(Token::OpenCurly);
            case '}':
                return this->readPlainToken(Token::CloseCurly);
            case '[':
                return this->readPlainToken(Token::OpenSquare);
            case ']':
                return this->readPlainToken(Token::CloseSquare);
            case ':':
                return this->readPlainToken(Token::Colon);
            case '=':
                return this->readPlainToken(Token::Equals);
            case ',':
                return this->readPlainToken(Token::Comma);
            case ';':
                return this->readPlainToken(Token::Semicolon);

            case '"':
            case '\'': {
                Token token = {Token::Text, this->readOfs, {}};
                ViewStream in{this->srcView.substr(this->readOfs)};
                token.text = readQuotedString(in, QuotedStringType::JSON, true,
                    [this, &in](QuotedStringError errorCode) {
                        u32 fileOfs = numericCast<u32>(in.curByte - this->srcView.bytes());
                        switch (errorCode) {
                            case QuotedStringError::UnexpectedEndOfLine:
                                this->error(fileOfs, "Unexpected end of line in string literal");
                                break;
                            case QuotedStringError::UnexpectedEndOfFile:
                                this->error(fileOfs, "Unexpected end of file in string literal");
                                break;
                            case QuotedStringError::BadEscapeSequence:
                                this->error(fileOfs, "Bad escape sequence in string literal");
                                break;
                            case QuotedStringError::NoOpeningQuote:
                                this->error(fileOfs, "Expected opening quote in string literal");
                                break;
                        }
                    });
                this->readOfs = numericCast<u32>(in.curByte - this->srcView.bytes());
                this->nextUnit = (this->readOfs < this->srcView.numBytes()) ? this->srcView[this->readOfs] : -1;
                if (in.inputError)
                    return {};
                return token;
            }

            default:
                if (isAlnumUnit(this->nextUnit))
                    return this->readLiteral();
                else
                    return {Token::Junk, this->readOfs, {}};
        }
    }
}

// FIXME: Maybe turn this into a format string because it's common
String escape(StringView str) {
    MemStream out;
    printEscapedString(out, str);
    return out.moveToString();
}

String Parser::toString(const Token& token) {
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

String Parser::toString(const Node& node) {
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

Node Parser::readObject(const Token& startToken) {
    PLY_ASSERT(startToken.type == Token::OpenCurly);
    ScopeHandler objectScope{*this, ParseError::Scope::object(startToken.fileOfs)};
    Node node{Node::Object{}, startToken.fileOfs};
    Token prevProperty = {};
    for (;;) {
        bool gotSeparator = false;
        Token firstToken = {};
        for (;;) {
            firstToken = this->readToken(true);
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
                this->error(firstToken.fileOfs, String::format("Expected a comma, semicolon or newline "
                                                               "separator between properties \"{}\" and \"{}\"",
                                                               escape(prevProperty.text), escape(firstToken.text)));
                return {};
            }
        } else if (prevProperty.isValid()) {
            this->error(firstToken.fileOfs, String::format("Unexpected {} after property \"{}\"", toString(firstToken),
                                                           escape(prevProperty.text)));
            return {};
        } else {
            this->error(firstToken.fileOfs, String::format("Expected property, got {}", toString(firstToken)));
            return {};
        }

        const Node& existingNode = node.get(firstToken.text);
        if (existingNode.isValid()) {
            ScopeHandler duplicateScope{*this, ParseError::Scope::duplicate(existingNode.fileOfs)};
            this->error(firstToken.fileOfs, String::format("Duplicate property \"{}\"", escape(firstToken.text)));
            return {};
        }

        Token colon = this->readToken();
        if (colon.type != Token::Colon && colon.type != Token::Equals) {
            this->error(colon.fileOfs, String::format("Expected \":\" or \"=\" after \"{}\", got {}",
                                                      escape(firstToken.text), toString(colon)));
            return {};
        }

        {
            // Read value of property
            ScopeHandler propertyScope{*this, ParseError::Scope::property(firstToken.fileOfs, firstToken.text)};
            Node value = this->readExpression(this->readToken(), &colon);
            if (!value.isValid())
                return value;
            node.set(firstToken.text, std::move(value));
        }

        prevProperty = std::move(firstToken);
    }
    return {};
}

Node Parser::readArray(const Token& startToken) {
    PLY_ASSERT(startToken.type == Token::OpenSquare);
    ScopeHandler arrayScope{*this, ParseError::Scope::array(startToken.fileOfs, 0)};
    Node arrayNode{Node::Array{}, startToken.fileOfs};
    Token sepTokenHolder;
    Token* sepToken = nullptr;
    for (;;) {
        Token token = this->readToken(true);
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
                Node value = this->readExpression(std::move(token), sepToken);
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

Node Parser::readExpression(Token&& firstToken, const Token* afterToken) {
    switch (firstToken.type) {
        case Token::OpenCurly:
            return this->readObject(firstToken);

        case Token::OpenSquare:
            return this->readArray(firstToken);

        case Token::Text: {
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
            mout.format("Unexpected {} after {}", toString(firstToken), afterToken ? toString(*afterToken) : "");
            this->error(firstToken.fileOfs, mout.moveToString());
            return {};
        }
    }
}

Parser::Result Parser::parse(StringView path, StringView srcView) {
    this->srcView = srcView;
    this->nextUnit = this->srcView.numBytes() > 0 ? this->srcView[0] : -1;

    this->tokenLocMap = TokenLocationMap::createFromString(srcView);

    Token rootToken = this->readToken();
    Node root = this->readExpression(std::move(rootToken));
    if (!root.isValid())
        return {{}, std::move(this->tokenLocMap), this->readOfs};

    if (this->greedy) {
        Token nextToken = this->readToken();
        if (nextToken.type != Token::EndOfFile) {
            this->error(nextToken.fileOfs, String::format("Unexpected {} after {}", toString(nextToken), toString(root)));
            return {{}, std::move(this->tokenLocMap), this->readOfs};
        }
    }

    return {std::move(root), std::move(this->tokenLocMap), this->readOfs};
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
