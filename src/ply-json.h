/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#pragma once
#include "ply-base.h"
#include "ply-tokenizer.h"

namespace ply {
namespace json {

//  ▄▄  ▄▄            ▄▄
//  ███ ██  ▄▄▄▄   ▄▄▄██  ▄▄▄▄
//  ██▀███ ██  ██ ██  ██ ██▄▄██
//  ██  ██ ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄▄
//

struct Node {
    enum class Type {
        Invalid = 0,
        Text,
        Array,
        Object,
    };

    Type type = Type::Invalid;
    u32 file_ofs = 0;

    struct Object {
        struct Item {
            String key;
            Owned<Node> value;

            StringView get_lookup_key() const {
                return this->key;
            }
        };

        Set<Item> items;
    };

    union {
        String text_;
        Array<Owned<Node>> array_;
        Object object_;
    };

    Node() {
    }
    Node(Type type, u32 file_ofs);
    ~Node();

    static Node InvalidNode;
    static Object EmptyObject;

    bool is_valid() const {
        return this->type != Type::Invalid;
    }

    //-----------------------------------------------------------
    // Text
    //-----------------------------------------------------------

    bool is_text() const {
        return this->type == Type::Text;
    }

    StringView text() const {
        if (this->type == Type::Text) {
            return this->text_;
        } else {
            return {};
        }
    }

    void set_text(String&& text) {
        if (this->type == Type::Text) {
            this->text_ = std::move(text);
        }
    }

    //-----------------------------------------------------------
    // Array
    //-----------------------------------------------------------

    bool is_array() const {
        return this->type == Type::Array;
    }

    Node* get(u32 i) {
        if (this->type != Type::Array)
            return &InvalidNode;
        if (i >= this->array_.num_items())
            return &InvalidNode;
        return this->array_[i];
    }

    const Node* get(u32 i) const {
        return const_cast<Node*>(this)->get(i);
    }

    ArrayView<const Node* const> array_view() const {
        if (this->type == Type::Array) {
            return reinterpret_cast<const Array<const Node*>&>(this->array_);
        } else {
            return {};
        }
    }

    Array<Owned<Node>>& array() {
        PLY_ASSERT(this->type == Type::Array);
        return this->array_;
    }

    //-----------------------------------------------------------
    // Object
    //-----------------------------------------------------------

    bool is_object() const {
        return this->type == Type::Object;
    }

    Node* get(StringView key);
    const Node* get(StringView key) const {
        return const_cast<Node*>(this)->get(key);
    }
    void set(StringView key, Owned<Node>&& value);
    void remove(StringView key);

    const Object& object() const {
        if (this->type == Type::Object) {
            return this->object_;
        } else {
            return EmptyObject;
        }
    }

    Object& object() {
        PLY_ASSERT(this->type == Type::Object);
        return this->object_;
    }
};

//  ▄▄▄▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██▄▄██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ▀█▄▄▄
//

struct ParseError {
    struct Scope {
        enum Type { Object, Property, Duplicate, Array };
        u32 file_ofs;
        Type type;
        StringView name;
        u32 index;

        static Scope object(u32 file_ofs) {
            return {file_ofs, Object, {}, 0};
        }
        static Scope property(u32 file_ofs, StringView name) {
            return {file_ofs, Property, name, 0};
        }
        static Scope duplicate(u32 file_ofs) {
            return {file_ofs, Duplicate, {}, 0};
        }
        static Scope array(u32 file_ofs, u32 index) {
            return {file_ofs, Array, {}, index};
        }
    };

    u32 file_ofs;
    String message;
    const Array<Scope>& context;
};

class Parser {
private:
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
            EndOfFile
        };
        Type type = Invalid;
        u32 file_ofs = 0;
        String text;

        bool is_valid() const {
            return type != Type::Invalid;
        }
    };

    Functor<void(const ParseError& err)> error_callback;
    TokenLocationMap token_loc_map;
    bool any_error_ = false;
    StringView src_view;
    u32 read_ofs = 0;
    s32 next_unit = 0;
    u32 tab_size = 4;
    Token push_back_token;
    Array<ParseError::Scope> context;

    void push_back(Token&& token) {
        push_back_token = std::move(token);
    }

    struct ScopeHandler {
        Parser& parser;
        u32 index;

        ScopeHandler(Parser& parser, ParseError::Scope&& scope) : parser{parser}, index{parser.context.num_items()} {
            parser.context.append(std::move(scope));
        }
        ~ScopeHandler() {
            // parser.context can be empty when Parse_Error is thrown
            if (!parser.context.is_empty()) {
                PLY_ASSERT(parser.context.num_items() == index + 1);
                parser.context.pop();
            }
        }
        ParseError::Scope& get() {
            return parser.context[index];
        }
    };

    void error(u32 file_ofs, String&& message);
    void advance_char();
    Token read_plain_token(Token::Type type);
    bool read_escaped_hex(Stream& out, u32 escape_file_ofs);
    Token read_quoted_string();
    Token read_literal();
    Token read_token(bool tokenize_new_line = false);
    static String to_string(const Token& token);
    static String to_string(const Node* node);
    Owned<Node> read_object(const Token& start_token);
    Owned<Node> read_array(const Token& start_token);
    Owned<Node> read_expression(Token&& first_token, const Token* after_token = nullptr);

public:
    void set_tab_size(int tab_size_) {
        tab_size = tab_size_;
    }
    void set_error_callback(Functor<void(const ParseError& err)>&& cb) {
        this->error_callback = std::move(cb);
    }
    bool any_error() const {
        return this->any_error_;
    }

    struct Result {
        Owned<Node> root;
        TokenLocationMap token_loc_map;
    };

    void dump_error(const ParseError& error, Stream& out) const;

    Result parse(StringView path, StringView src_view_);
};

//  ▄▄    ▄▄        ▄▄  ▄▄
//  ██ ▄▄ ██ ▄▄▄▄▄  ▄▄ ▄██▄▄  ▄▄▄▄
//  ▀█▄██▄█▀ ██  ▀▀ ██  ██   ██▄▄██
//   ██▀▀██  ██     ██  ▀█▄▄ ▀█▄▄▄
//

void write(Stream& out, const Node* a_node);
String to_string(const Node* a_node);

} // namespace json
} // namespace ply
