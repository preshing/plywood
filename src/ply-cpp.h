/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#pragma once
#include "ply-system.h"
#include "ply-tokenizer.h"

namespace ply {
namespace cpp {

//-----------------------------------------------------------
// The classes are designed to represent the contents of a C++ source code document.
// They're the nodes in the tree you get back when you call `parseFile`.
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
    Array<Owned<DeclSpecifier>> declSpecifiers;
    Owned<DeclProduction> abstractDcor;
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
        Token openAngle;
        Array<Arg> args;
        Token closeAngle;
    };
    struct Decltype {
        Token keyword;
        Token openParen;
        Owned<Expression> expr;
        Token closeParen;
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
        Token operatorKeyword;
        Array<Owned<DeclSpecifier>> declSpecifiers;
        Owned<DeclProduction> abstractDcor;
    };

    struct Prefix {
        Variant<Identifier, TemplateID, Decltype> var;
        Token doubleColon;
    };

    Array<Prefix> prefix;
    Variant<Identifier, TemplateID, Decltype, Destructor, OperatorFunc, ConversionFunc> var;

    bool isEmpty() const {
        return this->prefix.isEmpty() && this->var.isEmpty();
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
        Token equalSign;
        Variant<Owned<Expression>, TypeID> var;
    };
    struct FunctionBody {
        struct MemberInitializer {
            QualifiedID qid;
            Token openCurly;
            Owned<Expression> expr;
            Token closeCurly;
            Token comma;
        };
        Token colon;
        Array<MemberInitializer> memberInits;
        Token openCurly;
        Array<Statement> statements;
        Token closeCurly;
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
        Token externKeyword;
        Token literal;
    };
    struct Enum {
        struct Item {
            Token text;
            Initializer init;
            Token comma;
        };
        Token keyword;
        Token classKeyword;
        QualifiedID qid;
        Token colon;
        QualifiedID base;
        Token openCurly;
        Array<Item> enumerators;
        Token closeCurly;
    };
    struct Class {
        struct BaseSpecifier {
            Token accessSpec;
            QualifiedID baseQid;
            Token comma;
        };
        Token keyword;
        QualifiedID qid;
        Array<Token> virtSpecifiers;
        Token colon;
        Array<BaseSpecifier> baseSpecifiers;
        Token openCurly;
        Array<Declaration> memberDecls;
        Token closeCurly;
    };
    struct TypeSpecifier {
        Token elaborateKeyword; // Could be typename, class, struct, union or enum
        QualifiedID qid;
        // wasAssumed will be true whenever the parser makes a (possibly wrong) assumption due to
        // lack of type knowledge. For example:
        //      void func(int(A));
        //                    ^
        // If the parser does not definitively know whether A identifies a type, it will assume that
        // it is a type and set "was_assumed" to true. The first parameter of func will be parsed as
        // an unnamed function that takes an unnamed parameter of type A and returns int, instead of
        // as an integer named A, which is how it would have been parsed if A did not identify a
        // type.
        bool wasAssumed = false;
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
    Array<Owned<DeclSpecifier>> declSpecifiers; // Do these have to be Owned?
    Token identifier;
    Owned<DeclProduction> prod;
    Initializer init;
    Token comma;
};

struct DeclProduction {
    struct Parenthesized {
        Token openParen;
        Token closeParen;
    };
    struct Indirection {
        Array<QualifiedID::Prefix> prefix;
        Token punc;
    };
    struct ArrayOf {
        Token openSquare;
        Owned<Expression> size;
        Token closeSquare;
    };
    struct Function {
        Token openParen;
        Array<Parameter> params;
        Token closeParen;
        Array<Token> qualifiers;
        Token arrow;
        TypeID trailingRetType;
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
        Token externKeyword;
        Token literal;
        Token openCurly;
        Array<Declaration> childDecls;
        Token closeCurly;
    };
    struct Namespace {
        Token keyword;
        QualifiedID qid;
        Token openCurly;
        Array<Declaration> childDecls;
        Token closeCurly;
    };
    struct Entity {
        Array<Owned<DeclSpecifier>> declSpecifiers;
        Array<InitDeclarator> initDeclarators;
    };
    struct Template {
        Token keyword;
        Token openAngle;
        Array<Parameter> params;
        Token closeAngle;
        Owned<Declaration> childDecl;
    };
    struct TypeAlias {
        Token usingKeyword;
        Token name;
        Token equals;
        TypeID typeId;
    };
    struct UsingNamespace {
        Token usingKeyword;
        Token namespaceKeyword;
        QualifiedID qid;
    };
    struct StaticAssert {
        Token keyword;
        Token openParen;
        Array<Owned<Expression>> args;
        Token closeParen;
    };
    struct AccessSpecifier {
        Token keyword;
        Token colon;
    };

    Variant<Linkage, Namespace, Entity, Template, TypeAlias, UsingNamespace, StaticAssert, AccessSpecifier> var;
    Token semicolon;

    Token getFirstToken() const;
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
        Token openParen;
        Array<Owned<Expression>> arguments;
        Token closeParen;
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
    StringView absPath;
    u32 line = 0;
    u32 column = 0;
};

// Each TokenSpan object represents either a single token or a space.
// The spaces are inserted automatically by Parser::syntaxHighlight according to Plywood's
// formatting rules.
struct TokenSpan {
    enum Color {
        None,
        Type,
        Symbol,
        Variable,
    };

    Color color = None;
    bool isSpace = false;
    const QualifiedID* qid = nullptr; // The QualifiedID that the token is part of, if any.
    Token token;                      // Only valid if isSpace is false.
};

struct Parser {
    Array<String> includePaths;
    Array<PreprocessorDefinition> predefinedDefs;

    // Preprocessing:
    PreprocessResult preprocess(StringView absPath, StringView src);

    // Parsing:
    ParseResult parseFile(StringView absPath, StringView src);
    Declaration parseDeclaration(StringView input, StringView enclosingClassName = {});
    FileLocation getFileLocation(u32 inputOffset) const;

    // Syntax highlighting:
    Array<TokenSpan> syntaxHighlight(const Declaration& decl) const;

    // Debug output:
    void dumpDeclaration(const Declaration& decl) const;

    // Create & destroy:
    static Owned<Parser> create();
    void destroy();
};

} // namespace cpp
} // namespace ply
