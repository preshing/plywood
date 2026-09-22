/*─────────────────────────────────────────────────────────────────┐
│                                                                  │
│     ____      Plywood C++ Runtime Library                        │
│    ╱   ╱╲     https://plywood.dev/                               │
│   ╱___╱╭╮╲                                                       │
│    └──┴┴┴┘    JSON Support                                       │
│               Documentation: docs/high-level/json-support.md     │
│                                                                  │
└─────────────────────────────────────────────────────────────────*/

#pragma once
#include "ply-system.h"
#include "ply-tokenizer.h"

namespace ply {
namespace json {

//  ▄▄  ▄▄            ▄▄
//  ███ ██  ▄▄▄▄   ▄▄▄██  ▄▄▄▄
//  ██▀███ ██  ██ ██  ██ ██▄▄██
//  ██  ██ ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄▄
//

struct Node {
    struct Bool {
        bool value = false;
    };
    struct Number {
        double value = 0;
    };
    struct Text {
        String text;
    };
    struct Array {
        ply::Array<Node> items;
    };
    struct Object {
        Map<String, Node> items;
    };

    u32 fileOfs = 0;
    Variant<Bool, Number, Text, Array, Object> var;

    Node() {
    }
    Node(const Bool& b, u32 fileOfs = 0) : fileOfs{fileOfs}, var{b} {
    }
    Node(const Number& n, u32 fileOfs = 0) : fileOfs{fileOfs}, var{n} {
    }
    Node(Text&& text, u32 fileOfs = 0) : fileOfs{fileOfs}, var{Text{std::move(text)}} {
    }
    Node(Array&& arr, u32 fileOfs = 0) : fileOfs{fileOfs}, var{std::move(arr)} {
    }
    Node(Object&& obj, u32 fileOfs = 0) : fileOfs{fileOfs}, var{std::move(obj)} {
    }

    static Node InvalidNode;
    static Object EmptyObject;

    bool isValid() const {
        return !this->var.isEmpty();
    }
    explicit operator bool() const {
        return !this->var.isEmpty();
    }

    //-----------------------------------------------------------
    // Bool
    //-----------------------------------------------------------

    bool isBool() const {
        return this->var.is<Bool>();
    }

    bool getBool() const {
        if (const Bool* b = this->var.as<Bool>())
            return b->value;
        return false;
    }

    void setBool(bool value) {
        this->var = Bool{value};
    }

    //-----------------------------------------------------------
    // Number
    //-----------------------------------------------------------

    bool isNumber() const {
        return this->var.is<Number>();
    }

    double getNumber() const {
        if (const Number* n = this->var.as<Number>())
            return n->value;
        return 0;
    }

    void setNumber(double value) {
        this->var = Number{value};
    }

    //-----------------------------------------------------------
    // Text
    //-----------------------------------------------------------

    bool isText() const {
        return this->var.is<Text>();
    }

    StringView text() const {
        if (const Text* txt = this->var.as<Text>())
            return txt->text;
        return {};
    }

    void setText(String&& text) {
        this->var = Text{std::move(text)};
    }

    //-----------------------------------------------------------
    // Array
    //-----------------------------------------------------------

    bool isArray() const {
        return this->var.is<Array>();
    }

    Node& get(u32 i) {
        if (Array* arr = this->var.as<Array>()) {
            if (i < arr->items.numItems())
                return arr->items[i];
        }
        return InvalidNode;
    }

    const Node& get(u32 i) const {
        return const_cast<Node*>(this)->get(i);
    }

    ArrayView<const Node> arrayView() const {
        if (const Array* arr = this->var.as<Array>())
            return arr->items;
        return {};
    }

    ply::Array<Node>& array() {
        Array* arr = this->var.as<Array>();
        PLY_ASSERT(arr);
        return arr->items;
    }

    //-----------------------------------------------------------
    // Object
    //-----------------------------------------------------------

    bool isObject() const {
        return this->var.is<Object>();
    }

    Node& get(StringView key);
    const Node& get(StringView key) const {
        return const_cast<Node*>(this)->get(key);
    }
    void set(StringView key, Node&& value);
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
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██▄▄██ ██  ▀▀
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ▀█▄▄▄  ██
//

struct ParseResult {
    Node root;
    TokenLocationMap tokenLocMap;
    u32 numBytes = 0;
};

struct ParseError {
    struct Scope {
        enum Type {
            Object,
            Property,
            Duplicate,
            Array,
        };
        u32 fileOfs;
        Type type;
        StringView name;
        u32 index;

        static Scope object(u32 fileOfs) {
            return {fileOfs, Object, {}, 0};
        }
        static Scope property(u32 fileOfs, StringView name) {
            return {fileOfs, Property, name, 0};
        }
        static Scope duplicate(u32 fileOfs) {
            return {fileOfs, Duplicate, {}, 0};
        }
        static Scope array(u32 fileOfs, u32 index) {
            return {fileOfs, Array, {}, index};
        }
    };

    u32 fileOfs;
    String message;
    const Array<Scope>& context;
};

struct Parser {
    // Create & destroy
    static Owned<Parser> create();
    void destroy();

    // Settings
    void setTabSize(int tabSize);
    void setGreedy(bool greedy);
    void setErrorCallback(Functor<void(const ParseError& err)>&& cb);

    // Parsing
    ParseResult parse(StringView path, StringView srcView);
    bool anyError() const;
    void dumpError(const ParseError& error, Stream& out) const;
};

//  ▄▄    ▄▄        ▄▄  ▄▄
//  ██ ▄▄ ██ ▄▄▄▄▄  ▄▄ ▄██▄▄  ▄▄▄▄
//  ▀█▄██▄█▀ ██  ▀▀ ██  ██   ██▄▄██
//   ██▀▀██  ██     ██  ▀█▄▄ ▀█▄▄▄
//

struct WriteOptions {
    bool includeWhitespace = true;
};

void write(Stream& out, const Node& node, const WriteOptions& options = {});
String toString(const Node& node, const WriteOptions& options = {});

} // namespace json
} // namespace ply
