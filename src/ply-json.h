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
    struct Invalid {};
    struct Text {
        String text;
    };
    struct Bool {
        bool value = false;
    };
    struct Array {
        ply::Array<Owned<Node>> items;
    };
    struct Object {
        Map<String, Owned<Node>> items;
    };

    u32 file_ofs = 0;
    Variant<Invalid, Text, Bool, Array, Object> var;

    Node() : var{Invalid{}} {
    }
    template <typename T>
    Node(T&& v, u32 file_ofs = 0) : file_ofs{file_ofs}, var{std::forward<T>(v)} {
    }

    static Node InvalidNode;
    static Object EmptyObject;

    bool is_valid() const {
        return !this->var.is<Invalid>();
    }

    //-----------------------------------------------------------
    // Text
    //-----------------------------------------------------------

    bool is_text() const {
        return this->var.is<Text>();
    }

    StringView text() const {
        if (const Text* txt = this->var.as<Text>())
            return txt->text;
        return {};
    }

    void set_text(String&& text) {
        this->var = Text{std::move(text)};
    }

    //-----------------------------------------------------------
    // Bool
    //-----------------------------------------------------------

    bool is_bool() const {
        return this->var.is<Bool>();
    }

    bool get_bool() const {
        if (const Bool* b = this->var.as<Bool>())
            return b->value;
        return false;
    }

    void set_bool(bool value) {
        this->var = Bool{value};
    }

    //-----------------------------------------------------------
    // Array
    //-----------------------------------------------------------

    bool is_array() const {
        return this->var.is<Array>();
    }

    Node* get(u32 i) {
        if (Array* arr = this->var.as<Array>()) {
            if (i < arr->items.num_items())
                return arr->items[i];
        }
        return &InvalidNode;
    }

    const Node* get(u32 i) const {
        return const_cast<Node*>(this)->get(i);
    }

    ArrayView<const Owned<Node>> array_view() const {
        if (const Array* arr = this->var.as<Array>())
            return arr->items;
        return {};
    }

    ply::Array<Owned<Node>>& array() {
        Array* arr = this->var.as<Array>();
        PLY_ASSERT(arr);
        return arr->items;
    }

    //-----------------------------------------------------------
    // Object
    //-----------------------------------------------------------

    bool is_object() const {
        return this->var.is<Object>();
    }

    Node* get(StringView key);
    const Node* get(StringView key) const {
        return const_cast<Node*>(this)->get(key);
    }
    void set(StringView key, Owned<Node>&& value);
    void remove(StringView key);

    const Object& object() const {
        if (const Object* obj = this->var.as<Object>())
            return *obj;
        return EmptyObject;
    }

    Object& object() {
        Object* obj = this->var.as<Object>();
        PLY_ASSERT(obj);
        return *obj;
    }
};

//  ▄▄▄▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██▄▄██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ▀█▄▄▄
//

struct ParseError {
    struct Scope {
        enum Type {
            Object,
            Property,
            Duplicate,
            Array,
        };
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
            EndOfFile,
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
