/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include "ply-tokenizer.h"

namespace ply {

//  ▄▄▄▄▄ ▄▄ ▄▄▄               ▄▄                         ▄▄   ▄▄                     ▄▄   ▄▄
//  ██    ▄▄  ██   ▄▄▄▄        ██     ▄▄▄▄   ▄▄▄▄  ▄▄▄▄  ▄██▄▄ ▄▄  ▄▄▄▄  ▄▄▄▄▄        ███▄███  ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀  ██  ██  ██▄▄██       ██    ██  ██ ██     ▄▄▄██  ██   ██ ██  ██ ██  ██       ██▀█▀██  ▄▄▄██ ██  ██
//  ██    ██ ▄██▄ ▀█▄▄▄  ▄▄▄▄▄ ██▄▄▄ ▀█▄▄█▀ ▀█▄▄▄ ▀█▄▄██  ▀█▄▄ ██ ▀█▄▄█▀ ██  ██ ▄▄▄▄▄ ██   ██ ▀█▄▄██ ██▄▄█▀
//                                                                                                   ██

inline void update_line_and_column(u32& line_number, u32& column_number, u32 code_point) {
    if (code_point == '\n') {
        line_number++;
        column_number = 1;
    } else if (code_point == '\t') {
        u32 tab_size = 4;
        column_number += tab_size - (column_number % tab_size);
    } else if (code_point >= 32) {
        column_number++;
    }
}

TokenLocationMap TokenLocationMap::create_from_string(StringView src) {
    ViewStream in{src};
    TokenLocationMap result;
    result.view = src;
    u32 line_number = 1;
    u32 column_number = 1;
    u32 line_start_ofs = 0;

    u32 ofs = 0;
    u32 next_chunk_ofs = 256;
    result.table.append({1, 0, 1, 0});
    for (;;) {
        DecodeResult decoded = decode_unicode(in, UTF8);
        if (decoded.num_bytes == 0)
            break;

        u32 next_ofs = ofs + decoded.num_bytes;
        if (next_ofs > next_chunk_ofs) {
            result.table.append({line_number, next_chunk_ofs - line_start_ofs, column_number, ofs - next_chunk_ofs});
            next_chunk_ofs += 256;
        }
        ofs = next_ofs;

        update_line_and_column(line_number, column_number, decoded.point);
        if (decoded.point == '\n') {
            line_start_ofs = ofs;
        }
    }
    if (ofs == next_chunk_ofs) {
        result.table.append({line_number, next_chunk_ofs - line_start_ofs, column_number, ofs - next_chunk_ofs});
    }
    return result;
}

TokenLocation TokenLocationMap::get_location_from_offset(u32 file_offset) const {
    PLY_ASSERT(file_offset <= this->view.num_bytes());
    const TokenLocation& file_location = this->table[file_offset >> 8];
    u32 chunk_ofs = file_offset & ~0xff;
    const char* line_start = this->view.bytes() + (chunk_ofs - file_location.num_bytes_into_line);
    StringView src = this->view;
    src = src.substr(chunk_ofs - file_location.num_bytes_into_column);
    const char* target = this->view.bytes() + file_offset;
    u32 line_number = file_location.line_number;
    u32 column_number = file_location.column_number;

    for (;;) {
        if (src.bytes() >= target) {
            u32 nb = numeric_cast<u32>(target - src.bytes());
            // FIXME: num_bytes_into_line is incorrect here:
            return {line_number, numeric_cast<u32>(target - line_start), column_number, nb};
        }

        DecodeResult decoded = decode_unicode(src, UTF8);
        src = src.substr(decoded.num_bytes);

        update_line_and_column(line_number, column_number, decoded.point);
    }
}

//  ▄▄▄▄▄▄        ▄▄
//    ██    ▄▄▄▄  ██  ▄▄  ▄▄▄▄  ▄▄▄▄▄
//    ██   ██  ██ ██▄█▀  ██▄▄██ ██  ██
//    ██   ▀█▄▄█▀ ██ ▀█▄ ▀█▄▄▄  ██  ██
//

StringView get_punctuation_string(Token::Type tok) {
    switch (tok) {
        case Token::OpenCurly:
            return "{";
        case Token::CloseCurly:
            return "}";
        case Token::OpenParen:
            return "(";
        case Token::CloseParen:
            return ")";
        case Token::OpenAngle:
            return "<";
        case Token::CloseAngle:
            return ">";
        case Token::OpenSquare:
            return "[";
        case Token::CloseSquare:
            return "]";
        case Token::Semicolon:
            return ";";
        case Token::SingleColon:
            return ":";
        case Token::DoubleColon:
            return "::";
        case Token::SingleEqual:
            return "=";
        case Token::DoubleEqual:
            return "==";
        case Token::NotEqual:
            return "!=";
        case Token::PlusEqual:
            return "+=";
        case Token::MinusEqual:
            return "-=";
        case Token::Comma:
            return ",";
        case Token::QuestionMark:
            return "?";
        case Token::ForwardSlash:
            return "/";
        case Token::Star:
            return "*";
        case Token::Percent:
            return "%";
        case Token::SingleAmpersand:
            return "&";
        case Token::DoubleAmpersand:
            return "&&";
        case Token::SingleVerticalBar:
            return "|";
        case Token::DoubleVerticalBar:
            return "||";
        case Token::SinglePlus:
            return "+";
        case Token::DoublePlus:
            return "++";
        case Token::SingleMinus:
            return "-";
        case Token::DoubleMinus:
            return "--";
        case Token::LeftShift:
            return "<<";
        case Token::RightShift:
            return ">>";
        case Token::Dot:
            return ".";
        case Token::Tilde:
            return "~";
        case Token::Hash:
            return "#";
        case Token::DoubleHash:
            return "##";
        case Token::Bang:
            return "!";
        case Token::Ellipsis:
            return "...";
        case Token::LineComment:
            return "//";
        case Token::CStyleComment:
            return "/*";
        case Token::LessThanOrEqual:
            return "<=";
        case Token::GreaterThanOrEqual:
            return ">=";
        case Token::Arrow:
            return "->";
        case Token::StarEqual:
            return "*=";
        case Token::SlashEqual:
            return "/=";
        case Token::Caret:
            return "^";
        default: {
            PLY_ASSERT(0);
            return "???";
        }
    }
}

StringView Token::to_string() const {
    switch (this->type) {
        case Token::Unrecognized:
        case Token::Whitespace:
        case Token::Identifier:
        case Token::StringLiteral:
        case Token::NumericLiteral: {
            return this->text;
        }
        case Token::EOF: {
            return "end-of-file";
        }
        default: {
            return get_punctuation_string(this->type);
        }
    }
}

//  ▄▄▄▄▄▄        ▄▄                   ▄▄
//    ██    ▄▄▄▄  ██  ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄ ▄▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄
//    ██   ██  ██ ██▄█▀  ██▄▄██ ██  ██ ██   ▄█▀  ██▄▄██ ██  ▀▀
//    ██   ▀█▄▄█▀ ██ ▀█▄ ▀█▄▄▄  ██  ██ ██ ▄██▄▄▄ ▀█▄▄▄  ██
//

inline void error(Tokenizer& tkr, const char* pos, String&& message) {
    if (tkr.error_callback) {
        tkr.error_callback(tkr.input_offset + numeric_cast<u32>(pos - tkr.start_byte), std::move(message));
    }
}

void read_numeric_literal(ViewStream& in) {
    // FIXME: Optionally skip line continuations inside numeric literals.
    if (in.make_readable() && (*in.cur_byte == '0')) {
        in.cur_byte++;
        if (in.make_readable() && (*in.cur_byte == 'x')) {
            in.cur_byte++;
            read_u64_from_text(in, 16); // FIXME: Wasteful to compute the number and not use it
            goto suffix;
        }
    }

    read_double_from_text(in);
suffix:
    if (in.make_readable() && (*in.cur_byte == 'f')) {
        in.cur_byte++;
    } else {
        if (in.make_readable() && (*in.cur_byte == 'U')) {
            in.cur_byte++;
        }
        if (in.make_readable() && (*in.cur_byte == 'L')) {
            in.cur_byte++;
            if (in.make_readable() && (*in.cur_byte == 'L')) {
                in.cur_byte++;
            }
        }
    }
}

void read_string_literal(Tokenizer& tkr, ViewStream& in, char quote_punc) {
    PLY_ASSERT((quote_punc == '"') || (quote_punc = '\''));
    for (;;) {
        if (!in.make_readable()) {
            error(tkr, in.cur_byte, "unexpected end-of-file in string literal");
            break;
        }
        char c = *in.cur_byte;
        in.cur_byte++;
        if (c == '\\') {
            if (!in.make_readable()) {
                error(tkr, in.cur_byte, "unexpected end-of-file in string literal");
                break;
            }
            in.cur_byte++;
        } else if (c == '\n') {
            error(tkr, in.cur_byte, "unexpected end-of-line in string literal");
            break;
        } else if (c == quote_punc)
            break;
    }
}

bool read_delimiter_and_raw_string_literal(Tokenizer& tkr, ViewStream& in) {
    PLY_ASSERT((in.num_remaining_bytes() > 0) && (*in.cur_byte == '"'));
    in.cur_byte++;

    // read delimiter
    const char* delimiter_start = in.cur_byte;
    for (;;) {
        if (!in.make_readable()) {
            // End of file while reading raw string delimiter
            error(tkr, in.cur_byte, "unexpected end-of-file in raw string delimiter");
            return false;
        }
        char c = *in.cur_byte;
        if (c == '(')
            break;
        // FIXME: Recognize more whitespace characters
        if (is_whitespace(c) || c == ')' || c == '\\') {
            // Invalid character in delimiter
            error(tkr, in.cur_byte, "invalid character in raw string delimiter");
            return false;
        }
        in.cur_byte++;
    }

    // FIXME: Enforce maximum length of delimiter (at most 16 characters)
    const char* delimiter_end = in.cur_byte;
    in.cur_byte++;

    // Read remainder of string
    for (;;) {
        if (!in.make_readable()) {
            // End of file in string literal
            error(tkr, in.cur_byte, "unexpected end-of-file in string literal");
            return false;
        }
        char c = *in.cur_byte;
        in.cur_byte++;
        if (c == ')') {
            // Try to match delimiter
            const char* d = delimiter_start;
            for (;;) {
                if (d == delimiter_end) {
                    if (!in.make_readable()) {
                        // End of file while matching closing "
                        error(tkr, in.cur_byte, "unexpected end-of-file in string literal");
                        return false;
                    }
                    c = *in.cur_byte;
                    if (c == '"') {
                        // End of string literal
                        in.cur_byte++;
                        return true;
                    }
                }
                if (!in.make_readable()) {
                    // End of file while matching delimiter
                    error(tkr, in.cur_byte, "unexpected end-of-file in string literal");
                    return false;
                }
                c = *in.cur_byte;
                in.cur_byte++;
                if (c != *d)
                    break; // No match here
                d++;
            }
        }
    }
}

Token::Type read_identifier_or_literal(Tokenizer& tkr, ViewStream& in) {
    // FIXME: Optionally skip line continuations inside here.
    // This implementation is a little too obfuscated anyway.
    PLY_ASSERT(in.num_remaining_bytes() > 0);

    u32 mask[8] = {0, 0, 0x87fffffe, 0x7fffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    mask[1] |= 0x10;      // '$'
    mask[1] |= 0x3ff0000; // accept digits (we already know the first character is non-digit)

    const char* start_byte = in.cur_byte;
    for (;;) {
        if (!in.make_readable()) {
            PLY_ASSERT(in.cur_byte != start_byte);
            return Token::Identifier;
        }
        char c = *in.cur_byte;
        if ((mask[(u8) c >> 5] & (1 << ((u8) c & 31))) == 0) {
            if (c == '"') {
                if (in.cur_byte == start_byte + 1 && *start_byte == 'R') {
                    read_delimiter_and_raw_string_literal(tkr, in);
                } else {
                    // Treat it as a string prefix
                    in.cur_byte++;
                    read_string_literal(tkr, in, c);
                }
                return Token::StringLiteral;
            } else {
                if (start_byte == in.cur_byte) {
                    // Garbage token
                    error(tkr, in.cur_byte, "garbage characters encountered");
                    in.cur_byte++;
                    return Token::Unrecognized;
                } else {
                    return Token::Identifier;
                }
            }
        }
        in.cur_byte++;
    }
}

Token read_token(Tokenizer& tkr, ViewStream& in) {
    Token token;
    token.input_offset = tkr.input_offset;
    token.type = Token::Unrecognized;
    if (!in.make_readable()) {
        token.type = Token::EOF;
        return token;
    }

    tkr.start_byte = in.cur_byte;
    bool was_at_start_of_line = tkr.state.at_start_of_line;
    tkr.state.at_start_of_line = false;
    auto can_read_2nd_char = [&]() {
        if (tkr.config.allow_line_continuations_in_all_tokens && (in.num_remaining_bytes() >= 2) && (*in.cur_byte == '\\') && (*(in.cur_byte + 1) == '\n')) {
            in.cur_byte += 2;
        }
        return in.make_readable();
    };

retry:
    char c = *in.cur_byte;
    switch (c) {
        case '\n':
        case '\r':
        case '\t':
        case ' ': {
            // Skip whitespace while keeping track of start of line
            token.type = Token::Whitespace;
            tkr.state.at_start_of_line = was_at_start_of_line;
            while (in.make_readable()) {
                switch (*in.cur_byte) {
                    case '\n':
                        tkr.state.at_start_of_line = true;
                    case '\r':
                    case '\t':
                    case ' ':
                        in.cur_byte++;
                        break;
                    case '\\':
                        if (tkr.config.allow_line_continuations_in_all_tokens && (in.num_remaining_bytes() >= 2) &&
                            (in.cur_byte[1] == '\n')) {
                            in.cur_byte += 2;
                            break;
                        }
                    default:
                        goto end_of_white;
                }
            }
        end_of_white:
            break;
        }

        case '#': {
            in.cur_byte++;
            if (was_at_start_of_line && tkr.config.tokenize_preprocessor_directives) {
                token.type = Token::PreprocessorDirective;
                // Read directive up to its terminating newline.
                for (;;) {
                    if (!in.make_readable())
                        break;
                    char c = *in.cur_byte++;
                    if (c == '\n')
                        break;
                    // Skip \ newline escapes.
                    if (c == '\\' && (in.num_remaining_bytes() > 0) && (*in.cur_byte == '\n')) {
                        in.cur_byte++;
                    }
                }
                tkr.state.at_start_of_line = true;
            } else {
                if (can_read_2nd_char() && (*in.cur_byte == '#')) {
                    in.cur_byte++;
                    token.type = Token::DoubleHash;
                } else {
                    token.type = Token::Hash;
                }
            }
            break;
        }

        case '/': {
            in.cur_byte++;
            token.type = Token::ForwardSlash;
            if (can_read_2nd_char()) {
                if ((*in.cur_byte == '/') && tkr.config.tokenize_line_comments) {
                    in.cur_byte++;
                    token.type = Token::LineComment;
                    read_line(in);
                    tkr.state.at_start_of_line = true;
                } else if ((*in.cur_byte == '*') && tkr.config.tokenize_c_style_comments) {
                    token.type = Token::CStyleComment;
                    in.cur_byte++;
                    for (;;) {
                        if (!in.make_readable()) {
                            error(tkr, in.cur_byte, "unexpected end-of-file in C-style comment");
                            break;
                        } else if (*in.cur_byte == '*') {
                            in.cur_byte++;
                            if (!in.make_readable()) {
                                error(tkr, in.cur_byte, "unexpected end-of-file in C-style comment");
                                break;
                            } else if (*in.cur_byte == '/') {
                                in.cur_byte++;
                                break;
                            }
                        } else {
                            in.cur_byte++;
                        }
                    }
                } else if (*in.cur_byte == '=') {
                    token.type = Token::SlashEqual;
                    in.cur_byte++;
                }
            }
            break;
        }

        case '{': {
            token.type = Token::OpenCurly;
            in.cur_byte++;
            break;
        }

        case '}': {
            token.type = Token::CloseCurly;
            in.cur_byte++;
            break;
        }

        case ';': {
            token.type = Token::Semicolon;
            in.cur_byte++;
            break;
        }

        case '(': {
            token.type = Token::OpenParen;
            in.cur_byte++;
            break;
        }

        case ')': {
            token.type = Token::CloseParen;
            in.cur_byte++;
            break;
        }

        case '<': {
            token.type = Token::OpenAngle;
            in.cur_byte++;
            if (can_read_2nd_char()) {
                if (*in.cur_byte == '<') {
                    token.type = Token::LeftShift;
                    in.cur_byte++;
                } else if (*in.cur_byte == '=') {
                    token.type = Token::LessThanOrEqual;
                    in.cur_byte++;
                }
            }
            break;
        }

        case '>': {
            token.type = Token::CloseAngle;
            in.cur_byte++;
            if (can_read_2nd_char()) {
                if (tkr.config.tokenize_right_shift && (*in.cur_byte == '>')) {
                    token.type = Token::RightShift;
                    in.cur_byte++;
                } else if (*in.cur_byte == '=') {
                    token.type = Token::GreaterThanOrEqual;
                    in.cur_byte++;
                }
            }
            break;
        }

        case '[': {
            token.type = Token::OpenSquare;
            in.cur_byte++;
            break;
        }

        case ']': {
            token.type = Token::CloseSquare;
            in.cur_byte++;
            break;
        }

        case ':': {
            token.type = Token::SingleColon;
            in.cur_byte++;
            if (can_read_2nd_char()) {
                if (*in.cur_byte == ':') {
                    token.type = Token::DoubleColon;
                    in.cur_byte++;
                }
            }
            break;
        }

        case ',': {
            token.type = Token::Comma;
            in.cur_byte++;
            break;
        }

        case '?': {
            token.type = Token::QuestionMark;
            in.cur_byte++;
            break;
        }

        case '=': {
            token.type = Token::SingleEqual;
            in.cur_byte++;
            if (can_read_2nd_char()) {
                if (*in.cur_byte == '=') {
                    token.type = Token::DoubleEqual;
                    in.cur_byte++;
                }
            }
            break;
        }

        case '*': {
            in.cur_byte++;
            token.type = Token::Star;
            if (can_read_2nd_char()) {
                if (*in.cur_byte == '=') {
                    token.type = Token::StarEqual;
                    in.cur_byte++;
                }
            }
            break;
        }

        case '%': {
            token.type = Token::Percent;
            in.cur_byte++;
            break;
        }

        case '&': {
            token.type = Token::SingleAmpersand;
            in.cur_byte++;
            if (can_read_2nd_char()) {
                if (*in.cur_byte == '&') {
                    token.type = Token::DoubleAmpersand;
                    in.cur_byte++;
                }
            }
            break;
        }

        case '|': {
            token.type = Token::SingleVerticalBar;
            in.cur_byte++;
            if (can_read_2nd_char()) {
                if (*in.cur_byte == '|') {
                    token.type = Token::DoubleVerticalBar;
                    in.cur_byte++;
                }
            }
            break;
        }

        case '+': {
            token.type = Token::SinglePlus;
            in.cur_byte++;
            if (can_read_2nd_char()) {
                if (*in.cur_byte == '+') {
                    token.type = Token::DoublePlus;
                    in.cur_byte++;
                } else if (*in.cur_byte == '=') {
                    token.type = Token::PlusEqual;
                    in.cur_byte++;
                }
            }
            break;
        }

        case '-': {
            token.type = Token::SingleMinus;
            in.cur_byte++;
            if (can_read_2nd_char()) {
                if (*in.cur_byte == '-') {
                    token.type = Token::DoubleMinus;
                    in.cur_byte++;
                } else if (*in.cur_byte == '=') {
                    token.type = Token::MinusEqual;
                    in.cur_byte++;
                } else if (*in.cur_byte == '>') {
                    token.type = Token::Arrow;
                    in.cur_byte++;
                }
            }
            break;
        }

        case '.': {
            token.type = Token::Dot;
            in.cur_byte++;
            if (can_read_2nd_char()) {
                if (in.cur_byte[0] == '.' && in.cur_byte[1] == '.') {
                    token.type = Token::Ellipsis;
                    in.cur_byte += 2;
                }
            }
            break;
        }

        case '~': {
            token.type = Token::Tilde;
            in.cur_byte++;
            break;
        }

        case '^': {
            token.type = Token::Caret;
            in.cur_byte++;
            break;
        }

        case '!': {
            token.type = Token::Bang;
            in.cur_byte++;
            if (can_read_2nd_char()) {
                if (*in.cur_byte == '=') {
                    token.type = Token::NotEqual;
                    in.cur_byte++;
                }
            }
            break;
        }

        case '\'': {
            if (tkr.config.tokenize_single_quoted_strings) {
                token.type = Token::StringLiteral;
                in.cur_byte++;
                read_string_literal(tkr, in, '\'');
            }
            break;
        }

        case '"': {
            if (tkr.config.tokenize_double_quoted_strings) {
                token.type = Token::StringLiteral;
                in.cur_byte++;
                read_string_literal(tkr, in, '"');
            }
            break;
        }

        case '\\': {
            if (tkr.config.allow_line_continuations_in_all_tokens && (in.num_remaining_bytes() >= 2) && (in.cur_byte[1] == '\n')) {
                in.cur_byte += 2;
                goto retry;
            }
            break;
        }
    }

    if (token.type == Token::Unrecognized) {
        if (c >= '0' && c <= '9') {
            token.type = Token::NumericLiteral;
            read_numeric_literal(in);
        } else {
            token.type = read_identifier_or_literal(tkr, in);
        }
    } else if (token.type >= Token::Punctuation) {
        // Get hardcoded punctuation string in case there was a mid-token line continuation.
        token.text = get_punctuation_string(token.type);
    }

    token.text = {tkr.start_byte, in.cur_byte};
    PLY_ASSERT(token.text.num_bytes() > 0);
    tkr.input_offset += token.text.num_bytes();
    tkr.start_byte = nullptr;
    return token;
}

} // namespace ply
