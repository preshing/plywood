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
    // num_bytes_into_column can be non-zero if the TokenLocation lands in the middle of a multibyte character.
    u32 line_number;
    u32 num_bytes_into_line;
    u32 column_number : 28;
    u32 num_bytes_into_column : 4;

    TokenLocation(u32 line_number, u32 num_bytes_into_line, u32 column_number, u32 num_bytes_into_column)
        : line_number{line_number}, num_bytes_into_line{num_bytes_into_line}, column_number{column_number},
          num_bytes_into_column{num_bytes_into_column} {
    }
};

struct TokenLocationMap {
    Array<TokenLocation> table;
    StringView view;

    static TokenLocationMap create_from_string(StringView view);
    TokenLocation get_location_from_offset(u32 file_offset) const;
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

    u32 input_offset = 0;
    Type type = Invalid;
    StringView text;

    StringView to_string() const;
    bool is_valid() const {
        return (this->type != Invalid) && (this->type != EOF);
    }
    explicit operator bool() const {
        return this->is_valid();
    }
    bool operator==(const Token& other) const {
        return (this->input_offset == other.input_offset) && (this->type == other.type);
    }
};

StringView get_punctuation_string(Token::Type tok);

//  ▄▄▄▄▄▄        ▄▄                   ▄▄
//    ██    ▄▄▄▄  ██  ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄ ▄▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄
//    ██   ██  ██ ██▄█▀  ██▄▄██ ██  ██ ██   ▄█▀  ██▄▄██ ██  ▀▀
//    ██   ▀█▄▄█▀ ██ ▀█▄ ▀█▄▄▄  ██  ██ ██ ▄██▄▄▄ ▀█▄▄▄  ██
//

struct Tokenizer {
    struct Config {
        bool tokenize_right_shift = true;
        bool tokenize_preprocessor_directives = false;
        bool tokenize_c_style_comments = true;
        bool tokenize_line_comments = true;
        bool tokenize_single_quoted_strings = true;
        bool tokenize_double_quoted_strings = true;
        bool allow_line_continuations_in_all_tokens = false;
    };
    struct State {
        bool at_start_of_line = true;
    };

    u32 input_offset = 0;
    Config config;
    Functor<void(u32 input_offset, String&& message)> error_callback;
    State state;

    const char* start_byte = nullptr; // Used internally
};

Token read_token(Tokenizer& tkr, ViewStream& in);

} // namespace ply