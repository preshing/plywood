/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#pragma once
#include "ply-base.h"

namespace ply {

//  ▄▄                         ▄▄   ▄▄                   ▄▄   ▄▄
//  ██     ▄▄▄▄   ▄▄▄▄  ▄▄▄▄  ▄██▄▄ ▄▄  ▄▄▄▄  ▄▄▄▄▄      ███▄███  ▄▄▄▄  ▄▄▄▄▄
//  ██    ██  ██ ██     ▄▄▄██  ██   ██ ██  ██ ██  ██     ██▀█▀██  ▄▄▄██ ██  ██
//  ██▄▄▄ ▀█▄▄█▀ ▀█▄▄▄ ▀█▄▄██  ▀█▄▄ ██ ▀█▄▄█▀ ██  ██     ██   ██ ▀█▄▄██ ██▄▄█▀
//                                                                      ██

struct TokenLocation {
    // numBytesIntoColumn can be non-zero if the TokenLocation lands in the middle of a multibyte character.
    u32 lineNumber;
    u32 numBytesIntoLine;
    u32 columnNumber : 28;
    u32 numBytesIntoColumn : 4;

    TokenLocation(u32 lineNumber, u32 numBytesIntoLine, u32 columnNumber, u32 numBytesIntoColumn)
        : lineNumber{lineNumber}, numBytesIntoLine{numBytesIntoLine}, columnNumber{columnNumber},
          numBytesIntoColumn{numBytesIntoColumn} {
    }
};

struct TokenLocationMap {
    Array<TokenLocation> table;
    StringView view;

    static TokenLocationMap createFromString(StringView view);
    TokenLocation getLocationFromOffset(u32 fileOffset) const;
};

//  ▄▄▄▄▄▄        ▄▄
//    ██    ▄▄▄▄  ██  ▄▄  ▄▄▄▄  ▄▄▄▄▄
//    ██   ██  ██ ██▄█▀  ██▄▄██ ██  ██
//    ██   ▀█▄▄█▀ ██ ▀█▄ ▀█▄▄▄  ██  ██
//

struct Token {
    enum Type {
        Invalid = 0,
        EOF,
        Unrecognized,
        Whitespace,
        CStyleComment,
        LineComment,
        PreprocessorDirective,
        StringLiteral,
        NumericLiteral,
        Identifier,
        Punctuation,
        OpenCurly = Punctuation,
        CloseCurly,
        OpenParen,
        CloseParen,
        OpenAngle,
        CloseAngle,
        LessThanOrEqual,
        GreaterThanOrEqual,
        OpenSquare,
        CloseSquare,
        Semicolon,
        SingleColon,
        DoubleColon,
        SingleEqual,
        DoubleEqual,
        NotEqual,
        PlusEqual,
        MinusEqual,
        Arrow,
        StarEqual,
        SlashEqual,
        Comma,
        QuestionMark,
        ForwardSlash,
        Star,
        Percent,
        SingleAmpersand,
        DoubleAmpersand,
        SingleVerticalBar,
        DoubleVerticalBar,
        SinglePlus,
        DoublePlus,
        SingleMinus,
        DoubleMinus,
        LeftShift,
        RightShift,
        Dot,
        Tilde,
        Caret,
        Hash,
        DoubleHash,
        Bang,
        Ellipsis,
    };

    u32 inputOffset = 0;
    Type type = Invalid;
    StringView text;

    StringView toString() const;
    bool isValid() const {
        return (this->type != Invalid) && (this->type != EOF);
    }
    explicit operator bool() const {
        return this->isValid();
    }
    bool operator==(const Token& other) const {
        return (this->inputOffset == other.inputOffset) && (this->type == other.type);
    }
};

StringView getPunctuationString(Token::Type tok);

//  ▄▄▄▄▄▄        ▄▄                   ▄▄
//    ██    ▄▄▄▄  ██  ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄ ▄▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄
//    ██   ██  ██ ██▄█▀  ██▄▄██ ██  ██ ██   ▄█▀  ██▄▄██ ██  ▀▀
//    ██   ▀█▄▄█▀ ██ ▀█▄ ▀█▄▄▄  ██  ██ ██ ▄██▄▄▄ ▀█▄▄▄  ██
//

struct Tokenizer {
    struct Config {
        bool tokenizeRightShift = true;
        bool tokenizePreprocessorDirectives = false;
        bool tokenizeCStyleComments = true;
        bool tokenizeLineComments = true;
        bool tokenizeSingleQuotedStrings = true;
        bool tokenizeDoubleQuotedStrings = true;
        bool allowLineContinuationsInAllTokens = false;
    };
    struct State {
        bool atStartOfLine = true;
    };

    u32 inputOffset = 0;
    Config config;
    Functor<void(u32 inputOffset, String&& message)> errorCallback;
    State state;

    const char* startByte = nullptr; // Used internally
};

Token readToken(Tokenizer& tkr, ViewStream& in);

} // namespace ply
