/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

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

Node::Node(Type type, u32 file_ofs) : type{type}, file_ofs{file_ofs} {
    switch (type) {
        case Type::Text: {
            new (&this->text_) String;
            break;
        }
        case Type::Array: {
            new (&this->array_) Array<Owned<Node>>;
            break;
        }
        case Type::Object: {
            new (&this->object_) Object;
            break;
        }
        default:
            break;
    }
}

Node::~Node() {
    switch (this->type) {
        case Type::Text: {
            this->text_.~String();
            break;
        }
        case Type::Array: {
            this->array_.~Array<Owned<Node>>();
            break;
        }
        case Type::Object: {
            this->object_.~Object();
            break;
        }
        default:
            break;
    }
}

PLY_NO_INLINE Node* Node::get(StringView key) {
    if (this->type != Type::Object)
        return &InvalidNode;

    Object::Item* item  = this->object_.items.find(key);
    if (!item)
        return &InvalidNode;

    return item->value;
}

PLY_NO_INLINE void Node::set(StringView key, Owned<Node>&& value) {
    if (this->type != Type::Object)
        return;

    this->object_.items.insert_item({key, std::move(value)});
}

PLY_NO_INLINE void Node::remove(StringView key) {
    if (this->type != Type::Object)
        return;

    this->object_.items.erase(key);
}

//  ▄▄▄▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██▄▄██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ▀█▄▄▄
//

bool is_alnum_unit(u32 c) {
    return (c == '_') || (c == '$') || (c == '-') || (c == '.') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || (c >= 128);
}

void Parser::dump_error(const ParseError& error, Stream& out) const {
    TokenLocation error_loc = this->token_loc_map.get_location_from_offset(error.file_ofs);
    out.format("({}, {}): error: {}\n", error_loc.line_number, error_loc.column_number, error.message);
    for (u32 i = 0; i < error.context.num_items(); i++) {
        const ParseError::Scope& scope = error.context.back(-(s32) i - 1);
        TokenLocation context_loc = this->token_loc_map.get_location_from_offset(scope.file_ofs);
        out.format("({}, {}) ", context_loc.line_number, context_loc.column_number);
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

void Parser::error(u32 file_ofs, String&& message) {
    if (this->error_callback) {
        ParseError err{file_ofs, std::move(message), context};
        this->error_callback(err);
    }
    this->any_error_ = true;
}

void Parser::advance_char() {
    if (this->read_ofs + 1 < this->src_view.num_bytes) {
        this->read_ofs++;
        this->next_unit = this->src_view.bytes[this->read_ofs];
    } else {
        this->next_unit = -1;
    }
}

Parser::Token Parser::read_plain_token(Token::Type type) {
    Token result = {type, this->read_ofs, {}};
    this->advance_char();
    return result;
}

bool Parser::read_escaped_hex(Stream& out, u32 escape_file_ofs) {
    PLY_ASSERT(0); // FIXME
    return false;
}

Parser::Token Parser::read_quoted_string() {
    PLY_ASSERT(this->next_unit == '"' || this->next_unit == '\'');
    Token token = {Token::Text, this->read_ofs, {}};
    MemStream out;
    s32 end_byte = this->next_unit;
    u32 quote_run = 1;
    bool multiline = false;
    this->advance_char();

    for (;;) {
        if (this->next_unit == end_byte) {
            this->advance_char();
            if (quote_run == 0) {
                if (multiline) {
                    quote_run++;
                } else {
                    break; // end of string
                }
            } else {
                quote_run++;
                if (quote_run == 3) {
                    if (multiline) {
                        break; // end of string
                    } else {
                        multiline = true;
                        quote_run = 0;
                    }
                }
            }
        } else {
            if (quote_run > 0) {
                if (multiline) {
                    for (u32 i = 0; i < quote_run; i++) {
                        out.write((char) end_byte);
                    }
                } else if (quote_run == 2) {
                    break; // empty string
                }
                quote_run = 0;
            }

            switch (this->next_unit) {
                case -1: {
                    error(this->read_ofs, "Unexpected end of file in string literal");
                    return {};
                }

                case '\r':
                case '\n': {
                    if (multiline) {
                        if (this->next_unit == '\n') {
                            out.write((char) this->next_unit);
                        }
                        this->advance_char();
                    } else {
                        this->error(this->read_ofs, "Unexpected end of line in string literal");
                        return {};
                    }
                    break;
                }

                case '\\': {
                    // Escape sequence
                    u32 escape_file_ofs = this->read_ofs;
                    this->advance_char();
                    s32 code = this->next_unit;
                    this->advance_char();
                    switch (code) {
                        case -1: {
                            this->error(this->read_ofs, "Unexpected end of file in string literal");
                            return {};
                        }

                        case '\r':
                        case '\n': {
                            this->error(this->read_ofs, "Unexpected end of line in string literal");
                            return {};
                        }

                        case '\\':
                        case '\'':
                        case '"': {
                            out.write((char) code);
                            break;
                        }

                        case 'r': {
                            out.write((char) '\r');
                            break;
                        }

                        case 'n': {
                            out.write((char) '\n');
                            break;
                        }

                        case 't': {
                            out.write((char) '\t');
                            break;
                        }

                        case 'x': {
                            if (!read_escaped_hex(out, escape_file_ofs))
                                return {}; // FIXME: Would be better to continue reading
                                           // the rest of the string
                            break;
                        }

                        default: {
                            this->error(escape_file_ofs, String::format("Unrecognized escape sequence \"\\{}\"", (char) code));
                            return {}; // FIXME: Would be better to continue reading the
                                       // rest of the string
                        }
                    }
                    break;
                }

                default: {
                    out.write((char) this->next_unit);
                    this->advance_char();
                    break;
                }
            }
        }
    }

    token.text = out.move_to_string();
    return token;
}

Parser::Token Parser::read_literal() {
    PLY_ASSERT(is_alnum_unit(this->next_unit));
    Token token = {Token::Text, this->read_ofs, {}};
    u32 start_ofs = this->read_ofs;

    while (is_alnum_unit(this->next_unit)) {
        this->advance_char();
    }

    token.text = StringView{(char*) this->src_view.bytes + start_ofs, this->read_ofs - start_ofs};
    return token;
}

Parser::Token Parser::read_token(bool tokenize_new_line) {
    if (this->push_back_token.is_valid()) {
        Token token = std::move(this->push_back_token);
        this->push_back_token = {};
        return token;
    }

    for (;;) {
        switch (this->next_unit) {
            case ' ':
            case '\t':
            case '\r':
                this->advance_char();
                break;

            case '\n': {
                u32 new_line_ofs = this->read_ofs;
                this->advance_char();
                if (tokenize_new_line)
                    return {Token::NewLine, new_line_ofs, {}};
                break;
            }

            case -1:
                return {Token::EndOfFile, this->read_ofs, {}};
            case '{':
                return this->read_plain_token(Token::OpenCurly);
            case '}':
                return this->read_plain_token(Token::CloseCurly);
            case '[':
                return this->read_plain_token(Token::OpenSquare);
            case ']':
                return this->read_plain_token(Token::CloseSquare);
            case ':':
                return this->read_plain_token(Token::Colon);
            case '=':
                return this->read_plain_token(Token::Equals);
            case ',':
                return this->read_plain_token(Token::Comma);
            case ';':
                return this->read_plain_token(Token::Semicolon);

            case '"':
            case '\'':
                return this->read_quoted_string();

            default:
                if (is_alnum_unit(this->next_unit))
                    return this->read_literal();
                else
                    return {Token::Junk, this->read_ofs, {}};
        }
    }
}

// FIXME: Maybe turn this into a format string because it's common
String escape(StringView str) {
    MemStream out;
    print_escaped_string(out, str);
    return out.move_to_string();
}

String Parser::to_string(const Token& token) {
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

String Parser::to_string(const Node* node) {
    switch ((Node::Type) node->type) {
        case Node::Type::Object:
            return "object";
        case Node::Type::Array:
            return "array";
        case Node::Type::Text:
            return String::format("text \"{}\"", escape(node->text()));
        default:
            PLY_ASSERT(0);
            return "???";
    }
}

Owned<Node> Parser::read_object(const Token& start_token) {
    PLY_ASSERT(start_token.type == Token::OpenCurly);
    ScopeHandler object_scope{*this, ParseError::Scope::object(start_token.file_ofs)};
    Owned<Node> node = Heap::create<Node>(Node::Type::Object, start_token.file_ofs);
    Token prev_property = {};
    for (;;) {
        bool got_separator = false;
        Token first_token = {};
        for (;;) {
            first_token = this->read_token(true);
            switch (first_token.type) {
                case Token::CloseCurly:
                    return node;

                case Token::Comma:
                case Token::Semicolon:
                case Token::NewLine:
                    got_separator = true;
                    break;

                default:
                    goto break_outer;
            }
        }

    break_outer:
        if (first_token.type == Token::Text) {
            if (prev_property.is_valid() && !got_separator) {
                this->error(first_token.file_ofs, String::format("Expected a comma, semicolon or newline "
                                                         "separator between properties \"{}\" and \"{}\"",
                                                         escape(prev_property.text), escape(first_token.text)));
                return {};
            }
        } else if (prev_property.is_valid()) {
            this->error(first_token.file_ofs, String::format("Unexpected {} after property \"{}\"", to_string(first_token),
                                                     escape(prev_property.text)));
            return {};
        } else {
            this->error(first_token.file_ofs, String::format("Expected property, got {}", to_string(first_token)));
            return {};
        }

        Node* existing_node = node->get(first_token.text);
        if (existing_node->is_valid()) {
            ScopeHandler duplicate_scope{*this, ParseError::Scope::duplicate(existing_node->file_ofs)};
            this->error(first_token.file_ofs, String::format("Duplicate property \"{}\"", escape(first_token.text)));
            return {};
        }

        Token colon = this->read_token();
        if (colon.type != Token::Colon && colon.type != Token::Equals) {
            this->error(colon.file_ofs, String::format("Expected \":\" or \"=\" after \"{}\", got {}", escape(first_token.text),
                                               to_string(colon)));
            return {};
        }

        {
            // Read value of property
            ScopeHandler property_scope{*this, ParseError::Scope::property(first_token.file_ofs, first_token.text)};
            Owned<Node> value = this->read_expression(this->read_token(), &colon);
            if (!value->is_valid())
                return value;
            node->set(first_token.text, std::move(value));
        }

        prev_property = std::move(first_token);
    }
    return {};
}

Owned<Node> Parser::read_array(const Token& start_token) {
    PLY_ASSERT(start_token.type == Token::OpenSquare);
    ScopeHandler array_scope{*this, ParseError::Scope::array(start_token.file_ofs, 0)};
    Owned<Node> array_node = Heap::create<Node>(Node::Type::Array, start_token.file_ofs);
    Token sep_token_holder;
    Token* sep_token = nullptr;
    for (;;) {
        Token token = this->read_token(true);
        switch (token.type) {
            case Token::CloseSquare:
                return array_node;

            case Token::Comma:
            case Token::Semicolon:
            case Token::NewLine:
                sep_token_holder = std::move(token);
                sep_token = &sep_token_holder;
                break;

            default: {
                Owned<Node> value = this->read_expression(std::move(token), sep_token);
                if (!value->is_valid())
                    return value;
                array_node->array().append(std::move(value));
                array_scope.get().index++;
                sep_token = nullptr;
                break;
            }
        }
    }
}

Owned<Node> Parser::read_expression(Token&& first_token, const Token* after_token) {
    switch (first_token.type) {
        case Token::OpenCurly:
            return this->read_object(first_token);

        case Token::OpenSquare:
            return this->read_array(first_token);

        case Token::Text: {
            Owned<Node> node = Heap::create<Node>(Node::Type::Text, first_token.file_ofs);
            node->text_ = std::move(first_token.text);
            return node;
        }

        case Token::Invalid:
            return {};

        default: {
            MemStream mout;
            mout.format("Unexpected {} after {}", to_string(first_token), after_token ? to_string(*after_token) : "");
            this->error(first_token.file_ofs, mout.move_to_string());
            return {};
        }
    }
}

Parser::Result Parser::parse(StringView path, StringView src_view_) {
    this->src_view = src_view_;
    this->next_unit = this->src_view.num_bytes > 0 ? this->src_view[0] : -1;

    this->token_loc_map = TokenLocationMap::create_from_string(src_view_);

    Token root_token = this->read_token();
    Owned<Node> root = this->read_expression(std::move(root_token));
    if (!root->is_valid())
        return {};

    Token next_token = this->read_token();
    if (next_token.type != Token::EndOfFile) {
        this->error(next_token.file_ofs, String::format("Unexpected {} after {}", to_string(next_token), to_string(root)));
        return {};
    }

    return {std::move(root), std::move(this->token_loc_map)};
}

//  ▄▄    ▄▄        ▄▄  ▄▄
//  ██ ▄▄ ██ ▄▄▄▄▄  ▄▄ ▄██▄▄  ▄▄▄▄
//  ▀█▄██▄█▀ ██  ▀▀ ██  ██   ██▄▄██
//   ██▀▀██  ██     ██  ▀█▄▄ ▀█▄▄▄
//

struct WriteContext {
    Stream& out;
    u32 indent_level = 0;

    WriteContext(Stream& out) : out{out} {
    }

    void indent() {
        for (u32 i = 0; i < this->indent_level; i++) {
            this->out.write("  ");
        }
    }

    void write(const Node* node) {
        if (!node) {
            this->out.write("null");
            return;
        }

        switch (node->type) {
            case Node::Type::Object: {
                this->out.write("{\n");
                this->indent_level++;
                const Node::Object& obj_node = node->object();
                for (u32 item_index = 0; item_index < obj_node.items.items.num_items(); item_index++) {
                    const Node::Object::Item& obj_item = obj_node.items.items[item_index];
                    indent();
                    this->out.format("\"{}\": ", escape(obj_item.key));
                    write(obj_item.value);
                    if (item_index < obj_node.items.items.num_items() - 1) {
                        this->out.write(',');
                    }
                    this->out.write('\n');
                }
                this->indent_level--;
                indent();
                this->out.write('}');
                break;
            }
            case Node::Type::Array: {
                this->out.write("[\n");
                this->indent_level++;
                ArrayView<const Node* const> arr_node = node->array_view();
                u32 num_items = arr_node.num_items();
                for (u32 i = 0; i < num_items; i++) {
                    indent();
                    write(arr_node[i]);
                    if (i < num_items - 1) {
                        this->out.write(',');
                    }
                    this->out.write('\n');
                }
                this->indent_level--;
                indent();
                this->out.write(']');
                break;
            }
            case Node::Type::Text: {
                this->out.format("\"{}\"", escape(node->text()));
                break;
            }
            default:
                this->out.write("null");
                break;
        }
    }
};

void write(Stream& out, const Node* node) {
    WriteContext ctx{out};
    ctx.write(node);
}

String to_string(const Node* node) {
    MemStream out;
    write(out, node);
    return out.move_to_string();
}

} // namespace json
} // namespace ply
