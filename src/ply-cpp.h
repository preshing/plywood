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
namespace cpp {

//-----------------------------------------------------------
// The classes are designed to represent the contents of a C++ source code document.
// They're the nodes in the tree you get back when you call `parse_file`.
// The tree is built using Plywood container classes, which is definitely not the most memory-efficient approach to
// representing a C++ document, especially given the number of `Variant` and `Owned` objects that are created, but it's
// still more memory-efficient than a Python implementation would be. C++ has a formal grammar specified in the ISO
// draft.
//
// These classes are designed to encapsulate that grammar.
//-----------------------------------------------------------

struct DeclSpecifier;
struct DeclProduction;
struct Declaration;
struct Expression;
struct Statement;

//-----------------------------------------------------------
// QualifiedID
//
// `QualifiedID` represents a function, variable, type or template name.
// This class corresponds to _qualified-id_ in the grammar.
//
//      x
//      Foo
//      Foo::x
//      Foo::Bar::x
//      Foo::operator int
//      Foo::~Foo
//
// Subtypes:
//      Identifier
//      TemplateID
//      Decltype
//-----------------------------------------------------------

struct TypeID {
    Array<Owned<DeclSpecifier>> decl_specifiers;
    Owned<DeclProduction> abstract_dcor;
};

struct QualifiedID {
    struct Identifier {
        Token name;
    };
    struct TemplateID {
        struct Arg {
            Variant<Owned<Expression>, TypeID> var;
            Token comma;
        };

        Token name;
        Token open_angle;
        Array<Arg> args;
        Token close_angle;
    };
    struct Decltype {
        Token keyword;
        Token open_paren;
        Owned<Expression> expr;
        Token close_paren;
    };
    struct Destructor {
        Token tilde;
        Token name;
    };
    struct OperatorFunc {
        Token keyword;
        Token punc;
        Token punc2;
    };
    struct ConversionFunc {
        Token operator_keyword;
        Array<Owned<DeclSpecifier>> decl_specifiers;
        Owned<DeclProduction> abstract_dcor;
    };

    struct Prefix {
        Variant<Identifier, TemplateID, Decltype> var;
        Token double_colon;
    };

    Array<Prefix> prefix;
    Variant<Identifier, TemplateID, Decltype, Destructor, OperatorFunc, ConversionFunc> var;

    bool is_empty() const {
        return this->prefix.is_empty() && this->var.is_empty();
    }
};

//-----------------------------------------------------------
// Init_Declarator
//
// An Init_Declarator describes a declaration, function parameter, template parameter or type
// id (as in an alias). Corresponds to decl-specifier or type-specifier in the grammar.
//
// Declarators are combined with an Array<Owned<Decl_Specifier>> to form a declaration, function
// parameter, template parameter or type id (as in an alias). Corresponds to declarator or
// abstract-declarator in the grammar.
//
// In the case of a variable declaration, there can be multiple declarators:
//
//      int x, y;
//          ^^^^
//
// In the case of an function parameter or template parameter, the Declarator can be abstract, which
// means that the parameter is unnamed (QualifiedID is blank), and there is only the optional
// Decl_Production chain which modifies the base type into a pointer, function, etc.
//      void func(int, char*);
//                   ^     ^
//
// In the case of a type alias, the Declarator is always abstract.
//      using Func = int();
//                      ^^
//-----------------------------------------------------------

struct Initializer {
    struct Assignment {
        Token equal_sign;
        Variant<Owned<Expression>, TypeID> var;
    };
    struct FunctionBody {
        struct MemberInitializer {
            QualifiedID qid;
            Token open_curly;
            Owned<Expression> expr;
            Token close_curly;
            Token comma;
        };
        Token colon;
        Array<MemberInitializer> member_inits;
        Token open_curly;
        Array<Statement> statements;
        Token close_curly;
    };
    struct BitField {
        Token colon;
        Owned<Expression> expr;
    };

    Variant<Assignment, FunctionBody, BitField> var;
};

struct DeclSpecifier {
    struct Keyword {
        Token token;
    };
    struct Linkage {
        Token extern_keyword;
        Token literal;
    };
    struct Enum {
        struct Item {
            Token text;
            Initializer init;
            Token comma;
        };
        Token keyword;
        Token class_keyword;
        QualifiedID qid;
        Token colon;
        QualifiedID base;
        Token open_curly;
        Array<Item> enumerators;
        Token close_curly;
    };
    struct Class {
        struct BaseSpecifier {
            Token access_spec;
            QualifiedID base_qid;
            Token comma;
        };
        Token keyword;
        QualifiedID qid;
        Array<Token> virt_specifiers;
        Token colon;
        Array<BaseSpecifier> base_specifiers;
        Token open_curly;
        Array<Declaration> member_decls;
        Token close_curly;
    };
    struct TypeSpecifier {
        Token elaborate_keyword; // Could be typename, class, struct, union or enum
        QualifiedID qid;
        // was_assumed will be true whenever the parser makes a (possibly wrong) assumption due to
        // lack of type knowledge. For example:
        //      void func(int(A));
        //                    ^
        // If the parser does not definitively know whether A identifies a type, it will assume that
        // it is a type and set "was_assumed" to true. The first parameter of func will be parsed as
        // an unnamed function that takes an unnamed parameter of type A and returns int, instead of
        // as an integer named A, which is how it would have been parsed if A did not identify a
        // type.
        bool was_assumed = false;
    };
    struct TypeParameter {
        Token keyword; // typename or class
        Token ellipsis;
    };
    struct Ellipsis {
        Token token;
    };

    Variant<Keyword, Linkage, Class, Enum, TypeSpecifier, TypeParameter, Ellipsis> var;
};

struct Parameter {
    Array<Owned<DeclSpecifier>> decl_specifiers; // Do these have to be Owned?
    Token identifier;
    Owned<DeclProduction> prod;
    Initializer init;
    Token comma;
};

struct DeclProduction {
    struct Parenthesized {
        Token open_paren;
        Token close_paren;
    };
    struct Indirection {
        Array<QualifiedID::Prefix> prefix;
        Token punc;
    };
    struct ArrayOf {
        Token open_square;
        Owned<Expression> size;
        Token close_square;
    };
    struct Function {
        Token open_paren;
        Array<Parameter> params;
        Token close_paren;
        Array<Token> qualifiers;
        Token arrow;
        TypeID trailing_ret_type;
    };
    struct Qualifier {
        Token keyword;
    };

    Variant<Parenthesized, Indirection, ArrayOf, Function, Qualifier> var;
    Owned<DeclProduction> child;
};

struct InitDeclarator {
    QualifiedID qid;
    Owned<DeclProduction> prod;
    Initializer init;
    Token comma;
};

//-----------------------------------------------------------
// Declaration
// Some tokens are omitted if implicit; ie. the keyword `namespace`, braces around child declaration lists, etc.
// May be empty.
//-----------------------------------------------------------

struct Declaration {
    struct Linkage {
        Token extern_keyword;
        Token literal;
        Token open_curly;
        Array<Declaration> child_decls;
        Token close_curly;
    };
    struct Namespace {
        Token keyword;
        QualifiedID qid;
        Token open_curly;
        Array<Declaration> child_decls;
        Token close_curly;
    };
    struct Entity {
        Array<Owned<DeclSpecifier>> decl_specifiers;
        Array<InitDeclarator> init_declarators;
    };
    struct Template {
        Token keyword;
        Token open_angle;
        Array<Parameter> params;
        Token close_angle;
        Owned<Declaration> child_decl;
    };
    struct TypeAlias {
        Token using_keyword;
        Token name;
        Token equals;
        TypeID type_id;
    };
    struct UsingNamespace {
        Token using_keyword;
        Token namespace_keyword;
        QualifiedID qid;
    };
    struct StaticAssert {
        Token keyword;
        Token open_paren;
        Array<Owned<Expression>> args;
        Token close_paren;
    };
    struct AccessSpecifier {
        Token keyword;
        Token colon;
    };

    Variant<Linkage, Namespace, Entity, Template, TypeAlias, UsingNamespace, StaticAssert, AccessSpecifier> var;
    Token semicolon;

    Token get_first_token() const;
};

//-----------------------------------------------------------
// Expressions
//-----------------------------------------------------------

struct FunctionCall {};

struct Expression {
    struct Unary {
        Token punc;
        Owned<Expression> expr;
    };
    struct Binary {
        Token punc;
        Owned<Expression> expr1;
        Owned<Expression> expr2;
    };
    struct FunctionCall {
        Owned<Expression> callee;
        Token open_paren;
        Array<Owned<Expression>> arguments;
        Token close_paren;
    };
    struct Lambda {};

    Variant<QualifiedID, Unary, Binary, FunctionCall> subtype;
};

//-----------------------------------------------------------
// Statements
//-----------------------------------------------------------

struct Statement {
    struct Nested {};
    struct ForLoop {};
    struct WhileLoop {};
    struct Switch {};
    struct Goto {};
    struct Label {};
    struct Break {};
    struct Continue {};

    Variant<Declaration, Owned<Expression>, Nested, ForLoop, WhileLoop, Switch, Goto, Label, Break, Continue> subtype;
};

//-----------------------------------------------------------
// Public API
//-----------------------------------------------------------

struct PreprocessorDefinition {
    String name;
    String expansion;
};

struct PreprocessResult {
    bool success = false;
    String output;
    Array<String> diagnostics;
};

struct ParseResult {
    bool success = false;
    Array<Declaration> declarations;
    Array<String> diagnostics;
};

struct FileLocation {
    StringView abs_path;
    u32 line = 0;
    u32 column = 0;
};

// Each TokenSpan object represents either a single token or a space.
// The spaces are inserted automatically by Parser::syntax_highlight according to Plywood's
// formatting rules.
struct TokenSpan {
    enum Color {
        None,
        Type,
        Symbol,
        Variable,
    };

    Color color = None;
    bool is_space = false;
    const QualifiedID* qid = nullptr; // The QualifiedID that the token is part of, if any.
    Token token;                      // Only valid if is_space is false.
};

struct Parser {
    Array<String> include_paths;
    Array<PreprocessorDefinition> predefined_defs;

    // Preprocessing:
    PreprocessResult preprocess(StringView abs_path, StringView src);

    // Parsing:
    ParseResult parse_file(StringView abs_path, StringView src);
    Declaration parse_declaration(StringView input, StringView enclosing_class_name = {});
    FileLocation get_file_location(u32 input_offset) const;

    // Syntax highlighting:
    Array<TokenSpan> syntax_highlight(const Declaration& decl) const;

    // Debug output:
    void dump_declaration(const Declaration& decl) const;

    // Create & destroy:
    static Owned<Parser> create();
    void destroy();
};

} // namespace cpp
} // namespace ply
