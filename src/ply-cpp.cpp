/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include "ply-cpp.h"

namespace ply {
namespace cpp {

//  ▄▄▄▄▄
//  ██  ██ ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀▀  ██  ▀▀ ██▄▄██ ██  ██ ██  ▀▀ ██  ██ ██    ██▄▄██ ▀█▄▄▄  ▀█▄▄▄  ██  ██ ██  ▀▀
//  ██     ██     ▀█▄▄▄  ██▄▄█▀ ██     ▀█▄▄█▀ ▀█▄▄▄ ▀█▄▄▄   ▄▄▄█▀  ▄▄▄█▀ ▀█▄▄█▀ ██
//                       ██

struct Preprocessor {
    // All the files opened by the preprocessor (eg. through #include directives).
    struct File {
        String abs_path;
        StringView contents;
        String contents_storage;
        TokenLocationMap token_loc_map;
    };
    Array<File> files;

    // This B-tree lets us map any token to the chain of includes & macros that it came from.
    struct InputRange {
        // For each InputRange entry whose file_offset is 0, the location of the enclosing include directive or macro
        // invocation can be found by looking at the preceding InputRange in the BTree and calculating the file_offset
        // at the end of that range.
        // parent_start_offset tells us the input offset at the *start* of the enclosing file or macro expansion. There
        // should be an InputRange entry at this offset whose file_offset is 0 and whose file_or_macro_index matches the
        // InputRange entry preceding this one.
        u32 input_offset = 0;
        u32 is_macro_expansion : 1;
        u32 file_or_macro_index : 32;
        u32 file_offset = 0;
        s32 parent_range_index = -1;

        InputRange() : is_macro_expansion{0}, file_or_macro_index{0} {
        }
        u32 get_lookup_key() const {
            return this->input_offset;
        }
    };
    Array<InputRange> input_ranges;

    // The current include stack. Macro expansions are also pushed here when they are parsed.
    struct IncludedItem {
        u32 input_range_index = 0; // InputRange lookup key of enclosing directive or macro invocation.
        ViewStream vin;
    };
    Array<IncludedItem> include_stack;

    // All the preprocessor definitions that have been defined.
    struct Macro {
        StringView name;
        Map<StringView, u32> args;
        StringView expansion;
        u32 expansion_input_offset = u32(-1); // -1 means predefined.
        bool takes_arguments = false;

        StringView get_lookup_key() const {
            return this->name;
        }
    };
    Array<Macro> macros; // No item is ever erased, only replaced with a later item.
    Map<StringView, u32> macro_map;

    // This array holds string storage for:
    // - tokens joined by ## token pasting
    // - tokens joined  by \ line continuation
    Array<String> joined_token_storage;

    // Flags that influence tokenizer behavior.
    bool at_start_of_line = true;

    // This member is only valid when a token type of Macro is returned.
    // It'll remain valid until the next call to read_token.
    Array<Token> macro_args;
};

//  ▄▄▄▄▄                                     ▄▄▄▄                 ▄▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ██  ▄▄▄▄▄▄▄  ▄▄▄▄▄   ██
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██▄▄██ ██  ▀▀  ██  ██ ██ ██ ██  ██  ██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ▀█▄▄▄  ██     ▄██▄ ██ ██ ██ ██▄▄█▀ ▄██▄
//                                                          ██

enum ErrorType {
    Error,
    Warning,
    Note,
};

struct ParserImpl : Parser {
    Tokenizer tkr;
    Preprocessor pp;
    Array<String> diagnostics;
    bool is_only_preprocessing = false;
    bool success = true;

    // Backtracking and pushback
    struct PackedToken {
        Token::Type type = Token::EOF;
        u32 input_offset = 0;
    };
    static constexpr u32 NumTokensPerPage = 2048;
    Array<FixedArray<PackedToken, NumTokensPerPage + 1>> tokens;
    u32 token_index = 0;
    u32 num_tokens = 0;
    bool restore_point_enabled = false;

    // Status
    u32 pass_number = 1;

    //---------------------------
    // Error recovery
    static constexpr u32 AcceptOpenCurly = 0x1;
    static constexpr u32 AcceptCloseCurly = 0x2;
    static constexpr u32 AcceptCloseParen = 0x4;
    static constexpr u32 AcceptCloseSquare = 0x8;
    static constexpr u32 AcceptCloseAngle = 0x10;
    static constexpr u32 AcceptComma = 0x20;
    static constexpr u32 AcceptSemicolon = 0x40;

    u32 raw_error_count = 0; // Increments even when errors are muted.
    bool mute_errors = false;
    u32 outer_accept_flags = 0;

    //---------------------------

    ParserImpl();
    void error_no_mute(ErrorType type, u32 input_offset, StringView message);
    void error(ErrorType type, u32 input_offset, StringView message);
};

struct RestorePoint {
    ParserImpl* parser = nullptr;
    bool was_previously_enabled = false;
    u32 saved_token_index = 0;
    u32 saved_error_count = 0;

    RestorePoint(ParserImpl* parser) : parser{parser} {
        // Restore points can be nested. For example, when parsing the parameters of the
        // ply::Initializer constructor, there is a restore point when the constructor is
        // optimistically parsed, and another restore point after 'void' when we optimistically try
        // to parse a parameter list:
        //      struct Initializer {
        //          Initializer(void (*init)()) {
        //          ^                ^
        //          |                `---- second restore point
        //          `---- first restore point
        this->was_previously_enabled = parser->restore_point_enabled;
        parser->restore_point_enabled = true;
        this->saved_token_index = parser->token_index;
        this->saved_error_count = parser->raw_error_count;
    }
    ~RestorePoint() {
        if (this->parser) {
            this->cancel();
        }
    }
    bool error_occurred() const {
        return this->parser->raw_error_count != this->saved_error_count;
    }
    void backtrack() {
        PLY_ASSERT(this->parser); // Must not have been canceled
        this->parser->token_index = this->saved_token_index;
        this->parser->raw_error_count = this->saved_error_count;
    }
    void cancel() {
        PLY_ASSERT(this->parser);            // Must not have been canceled
        PLY_ASSERT(!this->error_occurred()); // no errors occurred
        this->parser->restore_point_enabled = this->was_previously_enabled;
        this->parser = nullptr;
    }
};

//---------------------------------------------------------------------------
// Error handling
//---------------------------------------------------------------------------

FileLocation get_file_location(const Preprocessor* pp, u32 input_offset) {
    s32 input_range_index = binary_search(pp->input_ranges, input_offset, FindGreaterThan) - 1;
    PLY_ASSERT(input_range_index >= 0);
    const Preprocessor::InputRange* input_range = &pp->input_ranges[input_range_index];
    while (input_range->is_macro_expansion) {
        input_range_index = input_range->parent_range_index;
        PLY_ASSERT(input_range_index >= 0);
        PLY_ASSERT(input_range_index + 1 < numeric_cast<s32>(pp->input_ranges.num_items()));
        PLY_ASSERT(pp->input_ranges[input_range_index + 1].parent_range_index == input_range_index);
        input_range = &pp->input_ranges[input_range_index];
        input_offset = pp->input_ranges[input_range_index + 1].input_offset;
    }
    const Preprocessor::File* file = &pp->files[input_range->file_or_macro_index];
    TokenLocation token_loc = file->token_loc_map.get_location_from_offset(
        numeric_cast<u32>(input_offset - input_range->input_offset + input_range->file_offset));
    return {file->abs_path, token_loc.line_number, token_loc.column_number};
}

String get_file_location_string(const Preprocessor* pp, u32 input_offset) {
    FileLocation file_location = get_file_location(pp, input_offset);
    return String::format("{}({}, {})", file_location.abs_path, file_location.line, file_location.column);
}

ParserImpl::ParserImpl() {
    this->tkr.config.tokenize_preprocessor_directives = true;
    this->tkr.error_callback = [this](u32 input_offset, String&& message) {
        // Tokenizer errors don't affect the raw error count.
        this->diagnostics.append(
            String::format("{}: error: {}\n", get_file_location_string(&this->pp, input_offset), message));
        this->success = false;
    };
}

void ParserImpl::error_no_mute(ErrorType type, u32 input_offset, StringView message) {
    if (type == Error) {
        this->raw_error_count++;
    }
    if (!this->restore_point_enabled && !this->mute_errors) {
        StringView type_str = "error";
        if (type == Warning) {
            type_str = "warning";
        } else if (type == Note) {
            type_str = "note";
        }
        this->diagnostics.append(
            String::format("{}: {}: {}\n", get_file_location_string(&this->pp, input_offset), type_str, message));
        if (type == Error) {
            this->success = false;
        }
    }
}

inline void ParserImpl::error(ErrorType type, u32 input_offset, StringView message) {
    this->error_no_mute(type, input_offset, message);
    this->mute_errors = true;
}

#define FMT_MSG(...) ((!parser->restore_point_enabled && !parser->mute_errors) ? String::format(__VA_ARGS__) : String{})

//---------------------------------------------------------
// Helpers
//---------------------------------------------------------
StringView get_text_at_offset(const Preprocessor* pp, u32 input_offset, u32 num_bytes) {
    s32 input_range_index = binary_search(pp->input_ranges, input_offset, FindGreaterThan) - 1;
    PLY_ASSERT(input_range_index >= 0);
    const Preprocessor::InputRange* input_range = &pp->input_ranges[input_range_index];
    if (input_range->is_macro_expansion) {
        const Preprocessor::Macro& macro = pp->macros[input_range->file_or_macro_index];
        return macro.expansion.substr(input_offset - input_range->input_offset + input_range->file_offset, num_bytes);
    } else {
        const Preprocessor::File& file = pp->files[input_range->file_or_macro_index];
        return file.contents.substr(input_offset - input_range->input_offset + input_range->file_offset, num_bytes);
    }
}

void include_file(ParserImpl* parser, StringView filename, u32 input_offset) {
    for (StringView include_path : parser->include_paths) {
        String full_path = join_path(include_path, filename);
        if (Filesystem::exists(full_path) == ER_FILE) {
            u32 file_index = parser->pp.files.num_items();
            Preprocessor::File& file = parser->pp.files.append();
            file.abs_path = full_path;
            file.contents_storage = Filesystem::load_text_autodetect(full_path);
            file.contents = file.contents_storage;
            file.token_loc_map = TokenLocationMap::create_from_string(file.contents);

            // Add to the include stack.
            Preprocessor::IncludedItem& item = parser->pp.include_stack.append();
            item.input_range_index = parser->pp.input_ranges.num_items();
            item.vin = ViewStream{file.contents};

            // Begin a new range of input.
            Preprocessor::InputRange& new_input_range = parser->pp.input_ranges.append();
            new_input_range.input_offset = input_offset;
            new_input_range.is_macro_expansion = 0;
            new_input_range.file_or_macro_index = file_index;
            new_input_range.parent_range_index = parser->pp.include_stack.back(-2).input_range_index;
        }
    }
}

void handle_preprocessor_directive(ParserImpl* parser, StringView directive, u32 input_offset) {
    ViewStream in{directive};
    StringView cmd = read_identifier(in);
    if (cmd == "include") {
        skip_whitespace(in);
        StringView rest = read_line(in);
        // FIXME: Do proper parsing of < > vs " "
        include_file(parser, rest.substr(1, rest.num_bytes() - 2), input_offset);
    } else if (cmd == "define") {
        // Parse macro name.
        skip_whitespace(in);
        StringView name = read_identifier(in);
        if (name.num_bytes() > 0) {
            // Parse macro expansion (may be empty).
            StringView expansion = in.view_remaining_bytes().trim();

            // Append new macro; don't erase old ones because existing InputRanges may still
            // reference them. Instead, update macro_map to point to the newest definition.
            u32 macro_idx = parser->pp.macros.num_items();
            Preprocessor::Macro& macro = parser->pp.macros.append();
            macro.name = name;
            macro.expansion = expansion;
            *parser->pp.macro_map.insert(name).value = macro_idx;
        }
    }
}

Token peek_token(ParserImpl* parser) {
    Token token;
    for (;;) {
        if (parser->token_index >= parser->num_tokens) {
            token = read_token(parser->tkr, parser->pp.include_stack.back().vin);
            if (token.type == Token::Identifier) {
                if (u32* macro_idx = parser->pp.macro_map.find(token.text)) {
                    // A preprocessor definition was found.
                    const Preprocessor::Macro& macro = parser->pp.macros[*macro_idx];

                    // We don't want the macro invocation itself to contribute to the logical input
                    // stream length. Rewind the tokenizer's logical offset so that the macro
                    // expansion logically starts at the beginning of the invocation token.
                    parser->tkr.input_offset = token.input_offset;

                    // Add to the include stack, which actually contains both includes and macros.
                    Preprocessor::IncludedItem& top = parser->pp.include_stack.append();
                    top.input_range_index = parser->pp.input_ranges.num_items();
                    top.vin = ViewStream{macro.expansion};

                    // Begin a new range of input.
                    Preprocessor::InputRange& new_input_range = parser->pp.input_ranges.append();
                    // The macro expansion occupies the same logical position as the invocation,
                    // so its InputRange starts at the invocation's input offset.
                    new_input_range.input_offset = token.input_offset;
                    new_input_range.is_macro_expansion = 1;
                    new_input_range.file_or_macro_index = *macro_idx;
                    new_input_range.parent_range_index = parser->pp.include_stack.back(-2).input_range_index;

                    // Macro invocations are *not* added to the parser's token list.
                    continue;
                }
            } else if (token.type == Token::EOF) {
                if (parser->pp.include_stack.num_items() > 1) {
                    // The last item in the include stack should correspond to the last input range.
                    PLY_ASSERT(parser->pp.include_stack.back().input_range_index ==
                               parser->pp.input_ranges.num_items() - 1);

                    // Begin a new input range for the remainder of the parent file or macro.
                    Preprocessor::InputRange& new_input_range = parser->pp.input_ranges.append();

                    // Sanity check the input offset of the EOF token.
                    const Preprocessor::InputRange& ending_input_range = parser->pp.input_ranges.back(-2);
                    PLY_ASSERT(ending_input_range.input_offset + (parser->pp.include_stack.back().vin.get_seek_pos() - ending_input_range.file_offset) == token.input_offset);

                    // Get the file offset where we are resuming the parent file or macro.
                    PLY_ASSERT(ending_input_range.parent_range_index ==
                               numeric_cast<s32>(parser->pp.include_stack.back(-2).input_range_index));
                    const Preprocessor::InputRange* old_parent_range = &parser->pp.input_ranges[ending_input_range.parent_range_index];
                    u32 old_parent_range_length = old_parent_range[1].input_offset - old_parent_range[0].input_offset;
                    u32 parent_file_seek = numeric_cast<u32>(parser->pp.include_stack.back(-2).vin.get_seek_pos());
                    // For includes (not macro expansions), the logical length of the parent
                    // segment should exactly match how far we've advanced in the parent file.
                    if (!ending_input_range.is_macro_expansion) {
                        PLY_ASSERT(old_parent_range->file_offset + old_parent_range_length == parent_file_seek);
                    }
                    
                    // Fill in the new input range.
                    new_input_range.input_offset = token.input_offset;
                    new_input_range.is_macro_expansion = old_parent_range->is_macro_expansion;
                    new_input_range.file_or_macro_index = old_parent_range->file_or_macro_index;
                    // Resume the parent at its current file (or macro) position.
                    new_input_range.file_offset = parent_file_seek;
                    new_input_range.parent_range_index = old_parent_range->parent_range_index;

                    // Pop the last item from the include stack.
                    parser->pp.include_stack.pop();
                    parser->pp.include_stack.back().input_range_index = parser->pp.input_ranges.num_items() - 1;
                }
            }

            // Add this token to the parser's token list. Preprocessor directives, comments and whitespace are added to
            // the token list, but not returned to the parser.
            u32 page_index = parser->token_index / ParserImpl::NumTokensPerPage;
            if (page_index >= parser->tokens.num_items()) {
                parser->tokens.append();
            }
            ParserImpl::PackedToken* packed =
                &parser->tokens[page_index][parser->token_index - page_index * ParserImpl::NumTokensPerPage];
            packed[0].type = token.type;
            packed[0].input_offset = token.input_offset;
            packed[1].input_offset = token.input_offset + token.text.num_bytes();
            parser->num_tokens++;

            // If it's a preprocessor directive, handle it.
            if (token.type == Token::PreprocessorDirective) {
                handle_preprocessor_directive(parser, token.text.substr(1).trim(), token.input_offset + token.text.num_bytes());
                // The directive may modify the include stack, so restart the loop to read the next token.
                parser->token_index++;
                continue;
            }
        } else {
            u32 page_index = parser->token_index / ParserImpl::NumTokensPerPage;
            u32 index_in_page = parser->token_index - page_index * ParserImpl::NumTokensPerPage;
            ParserImpl::PackedToken* packed = &parser->tokens[page_index][index_in_page];
            token.type = packed[0].type;
            token.input_offset = packed[0].input_offset;
            token.text = get_text_at_offset(&parser->pp, packed[0].input_offset,
                                            packed[1].input_offset - packed[0].input_offset);
        }

        switch (token.type) {
            case Token::PreprocessorDirective:
            case Token::CStyleComment:
            case Token::LineComment:
                parser->token_index++;
                break;

            case Token::Whitespace:
                if (parser->is_only_preprocessing)
                    return token;
                parser->token_index++;
                break;

            default:
                return token;
        }
    }
}

inline Token read_token(ParserImpl* parser) {
    Token token = peek_token(parser);
    parser->token_index++;
    return token;
}

bool ok_to_stay_in_scope(ParserImpl* parser, const Token& token) {
    switch (token.type) {
        case Token::OpenCurly: {
            if (parser->outer_accept_flags & ParserImpl::AcceptOpenCurly) {
                parser->token_index--;
                return false;
            }
            break;
        }
        case Token::CloseCurly: {
            if (parser->outer_accept_flags & ParserImpl::AcceptCloseCurly) {
                parser->token_index--;
                return false;
            }
            break;
        }
        case Token::CloseParen: {
            if (parser->outer_accept_flags & ParserImpl::AcceptCloseParen) {
                parser->token_index--;
                return false;
            }
            break;
        }
        case Token::CloseAngle: {
            if (parser->outer_accept_flags & ParserImpl::AcceptCloseAngle) {
                parser->token_index--;
                return false;
            }
            break;
        }
        case Token::CloseSquare: {
            if (parser->outer_accept_flags & ParserImpl::AcceptCloseSquare) {
                parser->token_index--;
                return false;
            }
            break;
        }
        case Token::EOF:
            return false;
        default:
            break;
    }
    return true;
}

struct SetAcceptFlagsInScope {
    ParserImpl* parser;
    u32 prev_accept_flags = 0;
    bool prev_tokenize_right_shift = false;

    SetAcceptFlagsInScope(ParserImpl* parser, Token::Type open_token_type) : parser{parser} {
        this->prev_accept_flags = parser->outer_accept_flags;
        this->prev_tokenize_right_shift = parser->tkr.config.tokenize_right_shift;

        switch (open_token_type) {
            case Token::OpenCurly: {
                parser->outer_accept_flags = ParserImpl::AcceptCloseCurly;
                parser->tkr.config.tokenize_right_shift = true;
                break;
            }
            case Token::OpenParen: {
                parser->outer_accept_flags =
                    (parser->outer_accept_flags | ParserImpl::AcceptCloseParen) & ~ParserImpl::AcceptCloseAngle;
                parser->tkr.config.tokenize_right_shift = true;
                break;
            }
            case Token::OpenAngle: {
                parser->outer_accept_flags = (parser->outer_accept_flags | ParserImpl::AcceptCloseAngle);
                parser->tkr.config.tokenize_right_shift = false;
                break;
            }
            case Token::OpenSquare: {
                parser->outer_accept_flags =
                    (parser->outer_accept_flags | ParserImpl::AcceptCloseSquare) & ~ParserImpl::AcceptCloseAngle;
                parser->tkr.config.tokenize_right_shift = true;
                break;
            }
            default: {
                PLY_ASSERT(0); // Illegal
                break;
            }
        }
    }

    ~SetAcceptFlagsInScope() {
        parser->outer_accept_flags = this->prev_accept_flags;
        parser->tkr.config.tokenize_right_shift = this->prev_tokenize_right_shift;
    }
};

//-------------------------------------------------------------------------------------
// skip_any_scope
//
// Returns false if an unexpected token is encountered and an outer scope is expected
// to handle it, as determined by parser->outer_accept_flags.
//-------------------------------------------------------------------------------------
bool skip_any_scope(ParserImpl* parser, Token* out_close_token, const Token& open_token) {
    SetAcceptFlagsInScope accept_scope{parser, open_token.type};
    Token::Type close_punc = (Token::Type) ((u32) open_token.type + 1);
    for (;;) {
        Token token = read_token(parser);
        if (token.type == close_punc) {
            if (out_close_token) {
                *out_close_token = token;
            }
            return true;
        }

        if (!ok_to_stay_in_scope(parser, token)) {
            parser->error_no_mute(
                Error, token.input_offset,
                FMT_MSG("expected '{}'", get_punctuation_string((Token::Type) ((u32) open_token.type + 1))));
            parser->error_no_mute(Note, open_token.input_offset, FMT_MSG("to match this '{}'", open_token.to_string()));
            parser->mute_errors = true;
            return false;
        }

        switch (token.type) {
            case Token::OpenAngle: {
                if (!parser->tkr.config.tokenize_right_shift) {
                    // If we were immediately inside a template-parameter/argument scope < >, treat
                    // < as a nested scope, because we now need to encounter two CloseAngle tokens:
                    skip_any_scope(parser, nullptr, token);
                }
                // If we are not immediately inside a template-parameter/argument scope < >, don't
                // treat < as the beginning of a scope, since it might just be a less-than operator.
                break;
            }
            case Token::OpenCurly:
            case Token::OpenParen:
            case Token::OpenSquare: {
                skip_any_scope(parser, nullptr, token);
                break;
            }
            default: {
            }
        }
    }
}

// Returns false if the given token was pushed back and ends an outer scope. Otherwise, it consumes
// the given token. If the given token begins a new scope, it consumes tokens until either the inner
// scope is closed, or until the inner scope is "canceled" by a closing token that closes an outer
// scope, as determined by parser->outer_accept_flags. In that case, the closing token is pushed
// back so that the caller can read it next. In each of those cases, it returns true to indicate to
// the caller that the given token was consumed and a new token is available to read.
bool handle_unexpected_token(ParserImpl* parser, Token* out_close_token, const Token& token) {
    // FIXME: Merge this with the second half of skip_any_scope:
    if (!ok_to_stay_in_scope(parser, token))
        return false;

    switch (token.type) {
        case Token::OpenAngle: {
            if (!parser->tkr.config.tokenize_right_shift) {
                // If we were immediately inside a template-parameter/argument scope < >, treat
                // < as a nested scope, because we now need to encounter two Close_Angle tokens:
                skip_any_scope(parser, out_close_token, token);
                // Ignore the return value of skip_any_scope. If it's false, that means some token
                // canceled the inner scope and was pushed back. We want the caller to read that
                // token next.
            }
            // If we are not immediately inside a template-parameter/argument scope < >, don't
            // treat < as the beginning of a scope, since it might just be a less-than operator.
            return true;
        }
        case Token::OpenCurly:
        case Token::OpenParen:
        case Token::OpenSquare: {
            skip_any_scope(parser, out_close_token, token);
            // Ignore the return value of skip_any_scope. If it's false, that means some token
            // canceled the inner scope and was pushed back. We want the caller to read that token
            // next.
            return true;
        }
        // FIXME: Log errors for unmatched closing brackets
        default: {
            return true;
        }
    }
}

bool close_scope(ParserImpl* parser, Token* out_close_token, const Token& open_token) {
    Token close_token = peek_token(parser);
    if (close_token.type == open_token.type + 1) {
        parser->token_index++;
        *out_close_token = close_token;
    } else {
        parser->error(Error, close_token.input_offset,
                      FMT_MSG("expected '{}' before '{}'", (open_token.type == Token::OpenSquare ? ']' : ')'),
                              close_token.to_string()));
        // Consume tokens up to the closing )
        if (!skip_any_scope(parser, nullptr, open_token)) {
            // We didn't get a closing ), but an outer scope will handle it
            PLY_ASSERT(parser->mute_errors);
            return false;
        }
        // Got closing )
        parser->mute_errors = false;
    }
    return true;
}

//----------------------------------------------
// Helpers
//----------------------------------------------

StringView get_class_name(const QualifiedID& qid) {
    if (auto* identifier = qid.var.as<QualifiedID::Identifier>()) {
        return identifier->name.text;
    } else if (auto* template_id = qid.var.as<QualifiedID::TemplateID>()) {
        return template_id->name.text;
    } else {
        return {};
    }
}

StringView get_ctor_dtor_name(const QualifiedID& qid) {
    if (auto* identifier = qid.var.as<QualifiedID::Identifier>()) {
        return identifier->name.text;
    } else if (auto* destructor = qid.var.as<QualifiedID::Destructor>()) {
        return destructor->name.text;
    } else if (auto* tmpl_spec = qid.var.as<QualifiedID::TemplateID>()) {
        return tmpl_spec->name.text;
    }
    return {};
}

String to_string(const QualifiedID& qid) {
    MemStream out;

    for (const QualifiedID::Prefix& comp : qid.prefix) {
        if (auto* ident = comp.var.as<QualifiedID::Identifier>()) {
            out.write(ident->name.text);
        } else if (auto* tmpl_spec = comp.var.as<QualifiedID::TemplateID>()) {
            out.format("{}<>", tmpl_spec->name.text);
        } else if (comp.var.is<QualifiedID::Decltype>()) {
            out.write("decltype()");
        } else {
            PLY_ASSERT(0);
        }
        out.write("::");
    }

    if (auto* identifier = qid.var.as<QualifiedID::Identifier>()) {
        out.write(identifier->name.text);
    } else if (auto* tmpl_spec = qid.var.as<QualifiedID::TemplateID>()) {
        out.format("{}<>", tmpl_spec->name.text);
    } else if (qid.var.is<QualifiedID::Decltype>()) {
        out.write("decltype()");
    } else if (auto* dtor = qid.var.as<QualifiedID::Destructor>()) {
        out.format("~{}", dtor->name.text);
    } else if (auto* op_func = qid.var.as<QualifiedID::OperatorFunc>()) {
        out.format("operator{}{}", op_func->punc.text, op_func->punc2.text);
    } else if (qid.var.is<QualifiedID::ConversionFunc>()) {
        // FIXME: improve this
        out.write("(conversion)");
    } else if (qid.var.is_empty()) {
        out.write("(empty)");
    } else {
        PLY_ASSERT(0);
    }

    return out.move_to_string();
}

// Used when logging errors
Token get_first_token(const QualifiedID& qid) {
    if (qid.prefix.num_items() > 0) {
        if (auto* identifier = qid.prefix[0].var.as<QualifiedID::Identifier>()) {
            return identifier->name;
        } else if (auto* tmpl_spec = qid.prefix[0].var.as<QualifiedID::TemplateID>()) {
            return tmpl_spec->name;
        } else if (auto* dt = qid.prefix[0].var.as<QualifiedID::Decltype>()) {
            return dt->keyword;
        }
        PLY_ASSERT(0); // Shouldn't be possible
    }

    if (qid.var.is_empty()) {
        return {};
    } else if (auto* identifier = qid.var.as<QualifiedID::Identifier>()) {
        return identifier->name;
    } else if (auto* tmpl_spec = qid.var.as<QualifiedID::TemplateID>()) {
        return tmpl_spec->name;
    } else if (auto* dt = qid.var.as<QualifiedID::Decltype>()) {
        return dt->keyword;
    } else if (auto* destructor = qid.var.as<QualifiedID::Destructor>()) {
        return destructor->tilde;
    } else if (auto* op_func = qid.var.as<QualifiedID::OperatorFunc>()) {
        return op_func->keyword;
    } else if (auto* conv_func = qid.var.as<QualifiedID::ConversionFunc>()) {
        return conv_func->operator_keyword;
    }
    PLY_ASSERT(0); // Shouldn't be possible
    return {};
}

Token get_first_token(const Declaration::Entity& entity) {
    if (!entity.decl_specifiers.is_empty()) {
        const DeclSpecifier& decl_spec = *entity.decl_specifiers[0];
        if (auto* keyword = decl_spec.var.as<DeclSpecifier::Keyword>()) {
            return keyword->token;
        } else if (auto* linkage = decl_spec.var.as<DeclSpecifier::Linkage>()) {
            return linkage->extern_keyword;
        } else if (auto* enum_ = decl_spec.var.as<DeclSpecifier::Enum>()) {
            return enum_->keyword;
        } else if (auto* class_ = decl_spec.var.as<DeclSpecifier::Class>()) {
            return class_->keyword;
        } else if (auto* type_spec = decl_spec.var.as<DeclSpecifier::TypeSpecifier>()) {
            if (type_spec->elaborate_keyword.is_valid())
                return type_spec->elaborate_keyword;
            return get_first_token(type_spec->qid);
        } else if (auto* type_param = decl_spec.var.as<DeclSpecifier::TypeParameter>()) {
            return type_param->keyword;
        } else if (auto* ellipsis = decl_spec.var.as<DeclSpecifier::Ellipsis>()) {
            return ellipsis->token;
        }
    }
    if (!entity.init_declarators.is_empty()) {
        const InitDeclarator& init_decl = entity.init_declarators[0];
        if (!init_decl.qid.is_empty()) {
            return get_first_token(init_decl.qid);
        }
    }
    PLY_ASSERT(0);
    return {};
}

Token Declaration::get_first_token() const {
    if (auto* linkage = this->var.as<Declaration::Linkage>()) {
        return linkage->extern_keyword;
    } else if (auto* namespace_ = this->var.as<Declaration::Namespace>()) {
        return namespace_->keyword;
    } else if (auto* entity = this->var.as<Declaration::Entity>()) {
        return cpp::get_first_token(*entity);
    } else if (auto* template_ = this->var.as<Declaration::Template>()) {
        return template_->keyword;
    } else if (auto* type_alias = this->var.as<Declaration::TypeAlias>()) {
        return type_alias->using_keyword;
    } else if (auto* using_namespace = this->var.as<Declaration::UsingNamespace>()) {
        return using_namespace->using_keyword;
    } else if (auto* static_assert_ = this->var.as<Declaration::StaticAssert>()) {
        return static_assert_->keyword;
    } else if (auto* access_spec = this->var.as<Declaration::AccessSpecifier>()) {
        return access_spec->keyword;
    }
    PLY_ASSERT(0);
    return {};
}

//-----------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------
enum class ParseQualifiedMode {
    AllowIncomplete,
    RequireComplete,
    RequireCompleteOrEmpty,
};

struct Declarator {
    Owned<DeclProduction> prod;
    QualifiedID qid;
};

struct DeclaratorFlags {
    static const u32 AllowNamed = 1;
    static const u32 AllowAbstract = 2;
};

struct ParsedExpression {
    Token start_token;
    Token end_token;
};

QualifiedID parse_qualified_id(ParserImpl* parser, ParseQualifiedMode mode);
void parse_declarator(ParserImpl* parser, Declarator& dcor, DeclProduction* nested, u32 dcor_flags);
void parse_optional_type_id_initializer(ParserImpl* parser, Initializer& result);
void parse_optional_variable_initializer(ParserImpl* parser, Initializer& result, bool allow_braced_init);
ParsedExpression parse_expression(ParserImpl* parser, bool optional = false);
Array<Declaration> parse_declaration_list(ParserImpl* parser, Token* out_close_curly, StringView enclosing_class_name);

//  ▄▄▄▄▄                       ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀
//   ▄▄▄▄                ▄▄▄  ▄▄   ▄▄▄ ▄▄            ▄▄     ▄▄▄▄ ▄▄▄▄▄
//  ██  ██ ▄▄  ▄▄  ▄▄▄▄   ██  ▄▄  ██   ▄▄  ▄▄▄▄   ▄▄▄██      ██  ██  ██  ▄▄▄▄
//  ██  ██ ██  ██  ▄▄▄██  ██  ██ ▀██▀▀ ██ ██▄▄██ ██  ██      ██  ██  ██ ▀█▄▄▄
//  ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄██ ▄██▄ ██  ██   ██ ▀█▄▄▄  ▀█▄▄██     ▄██▄ ██▄▄█▀  ▄▄▄█▀
//      ▀▀

TypeID parse_type_id(ParserImpl* parser) {
    TypeID result;
    s32 type_specifier_index = -1;
    for (;;) {
        Token token = peek_token(parser);
        if (token.type == Token::Identifier) {
            if (token.text == "const" || token.text == "volatile") {
                parser->mute_errors = false;
                parser->token_index++;
                result.decl_specifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Keyword{token}));
            } else {
                if (type_specifier_index < 0) {
                    parser->mute_errors = false;
                } else {
                    parser->error(Error, token.input_offset, "type-id cannot have a name");
                }
                type_specifier_index = result.decl_specifiers.num_items();
                DeclSpecifier* decl_spec = result.decl_specifiers.append(Heap::create<DeclSpecifier>());
                auto& type_spec = decl_spec->var.switch_to<DeclSpecifier::TypeSpecifier>();
                if (token.text == "typename" || token.text == "struct" || token.text == "class" ||
                    token.text == "union" || token.text == "enum") {
                    type_spec.elaborate_keyword = token;
                    parser->token_index++;
                }
                type_spec.qid = parse_qualified_id(parser, ParseQualifiedMode::RequireComplete);
            }
        } else {
            // Not an identifier. We should have parsed a type specifier by now.
            if (type_specifier_index < 0) {
                parser->error(Error, token.input_offset,
                              FMT_MSG("expected type specifier before '{}'", token.to_string()));
            }
            break;
        }
    }

    // Parse optional abstract declarator.
    Declarator dcor;
    parse_declarator(parser, dcor, nullptr, DeclaratorFlags::AllowAbstract);
    PLY_ASSERT(dcor.qid.is_empty());
    result.abstract_dcor = std::move(dcor.prod);
    return result;
}

// Consumes as much as it can; unrecognized tokens are returned to caller without logging an error
Array<QualifiedID::Prefix> parse_nested_name_specifier(ParserImpl* parser) {
    // FIXME: Support leading ::
    Array<QualifiedID::Prefix> prefix;
    for (;;) {
        QualifiedID::Prefix* comp = nullptr;

        Token token = peek_token(parser);
        if (token.type != Token::Identifier)
            break;

        if (token.text == "operator" || token.text == "const" || token.text == "volatile" || token.text == "inline" ||
            token.text == "static" || token.text == "friend")
            break;

        parser->token_index++;
        if (token.text == "decltype") {
            comp = &prefix.append();
            auto& dt = comp->var.switch_to<QualifiedID::Decltype>();
            dt.keyword = token;
            Token punc_token = peek_token(parser);
            if (punc_token.type == Token::OpenParen) {
                parser->token_index++;
                dt.open_paren = punc_token;
                skip_any_scope(parser, &dt.close_paren, punc_token);
            } else {
                // expected (
                parser->error(Error, punc_token.input_offset,
                              FMT_MSG("expected '(' before '{}'", punc_token.to_string()));
            }
        } else {
            comp = &prefix.append();
            Token punc_token = peek_token(parser);
            if (punc_token.type == Token::OpenAngle) {
                auto& tmpl_spec = comp->var.switch_to<QualifiedID::TemplateID>();
                tmpl_spec.name = token;
                parser->token_index++;
                // FIXME: We should only parse < as the start of a template-argument list if we know
                // that the preceding name refers to a template function or type. For now, we assume
                // it always does. If we ever start parsing function bodies, we won't be able to
                // assume this.
                if (parser->pass_number <= 1) {
                    tmpl_spec.open_angle = punc_token;

                    // Parse template-argument-list
                    SetAcceptFlagsInScope accept_scope{parser, Token::OpenAngle};
                    PLY_SET_IN_SCOPE(parser->tkr.config.tokenize_right_shift, false);

                    for (;;) {
                        // FIXME: Parse constant expressions here instead of only allowing type IDs

                        // Try to parse a type ID
                        auto& template_arg = tmpl_spec.args.append();
                        RestorePoint rp{parser};
                        TypeID type_id = parse_type_id(parser);
                        if (!rp.error_occurred()) {
                            // Successfully parsed a type ID
                            template_arg.var = std::move(type_id);
                        } else {
                            rp.backtrack();
                            rp.cancel();
                        }

                        for (;;) {
                            Token sep_token = read_token(parser);
                            if (sep_token.type == Token::CloseAngle) {
                                // End of template-argument-list
                                tmpl_spec.close_angle = sep_token;
                                parser->mute_errors = false;
                                goto break_args;
                            } else if (sep_token.type == Token::Comma) {
                                // Comma
                                template_arg.comma = sep_token;
                                parser->mute_errors = false;
                                break;
                            } else {
                                // Unexpected token
                                Token end_token;
                                if (!handle_unexpected_token(parser, &end_token, sep_token))
                                    goto break_outer;
                            }
                        }
                    }
                break_args:;
                } else {
                    PLY_FORCE_CRASH(); // FIXME: implement this
                }
            } else {
                auto& ident = comp->var.switch_to<QualifiedID::Identifier>();
                ident.name = token;
            }
        }

        PLY_ASSERT(comp);

        Token sep_token = peek_token(parser);
        if (sep_token.type != Token::DoubleColon)
            break;
        parser->token_index++;
        comp->double_colon = sep_token;
    }

break_outer:
    return prefix;
}

// Consumes as much as it can; unrecognized tokens are returned to caller without logging an error
QualifiedID parse_qualified_id(ParserImpl* parser, ParseQualifiedMode mode) {
    QualifiedID qid;
    qid.prefix = parse_nested_name_specifier(parser);
    if (qid.prefix.num_items() > 0) {
        QualifiedID::Prefix& tail = qid.prefix.back();
        if (!tail.double_colon.is_valid()) {
            if (auto* ident = tail.var.as<QualifiedID::Identifier>()) {
                qid.var = std::move(*ident);
            } else if (auto* tmpl_id = tail.var.as<QualifiedID::TemplateID>()) {
                qid.var = std::move(*tmpl_id);
            } else if (auto* dt = tail.var.as<QualifiedID::Decltype>()) {
                qid.var = std::move(*dt);
            }
            qid.prefix.pop();
        }
    }
    if (qid.var.is_empty()) {
        Token token = peek_token(parser);
        if (token.type == Token::Tilde) {
            parser->token_index++;
            Token token2 = peek_token(parser);
            if (token2.type != Token::Identifier) {
                // Expected class name after ~
                parser->error(Error, token2.input_offset,
                              FMT_MSG("expected destructor name before '{}'", token2.to_string()));
            } else {
                parser->token_index++;
                auto& dtor = qid.var.switch_to<QualifiedID::Destructor>();
                PLY_ASSERT(token2.text != "decltype"); // FIXME: Support this
                dtor.tilde = token;
                dtor.name = token2;
            }
        } else if (token.type == Token::Identifier) {
            if (token.text == "operator") {
                parser->token_index++;
                auto& op_func = qid.var.switch_to<QualifiedID::OperatorFunc>();
                op_func.keyword = token;
                Token op_token = read_token(parser);
                switch (op_token.type) {
                    case Token::LeftShift:
                    case Token::RightShift:
                    case Token::SinglePlus:
                    case Token::DoublePlus:
                    case Token::SingleMinus:
                    case Token::DoubleMinus:
                    case Token::Star:
                    case Token::Arrow:
                    case Token::ForwardSlash:
                    case Token::SingleEqual:
                    case Token::DoubleEqual:
                    case Token::NotEqual:
                    case Token::PlusEqual:
                    case Token::MinusEqual:
                    case Token::StarEqual:
                    case Token::SlashEqual:
                    case Token::OpenAngle:
                    case Token::CloseAngle:
                    case Token::LessThanOrEqual:
                    case Token::GreaterThanOrEqual:
                    case Token::OpenParen:
                    case Token::OpenSquare: {
                        op_func.punc = op_token;
                        if (op_token.type == Token::OpenParen) {
                            Token opToken2 = read_token(parser);
                            if (opToken2.type == Token::CloseParen) {
                                op_func.punc2 = opToken2;
                            } else {
                                // Expected ) after (
                                parser->error(Error, opToken2.input_offset,
                                              FMT_MSG("expected ')' before '{}'", opToken2.to_string()));
                                parser->token_index--;
                            }
                        } else if (op_token.type == Token::OpenSquare) {
                            Token opToken2 = read_token(parser);
                            if (opToken2.type == Token::CloseSquare) {
                                op_func.punc2 = opToken2;
                            } else {
                                parser->error(Error, opToken2.input_offset,
                                              FMT_MSG("expected ']' before '{}'", opToken2.to_string()));
                                parser->token_index--;
                            }
                        }
                        break;
                    }

                    default: {
                        // Expected operator token
                        parser->error(Error, op_token.input_offset,
                                      FMT_MSG("expected operator token before '{}'", op_token.to_string()));
                        parser->token_index--;
                        break;
                    };
                }
            }
        }
    }
    if (((mode == ParseQualifiedMode::RequireComplete) && qid.var.is_empty()) ||
        ((mode == ParseQualifiedMode::RequireCompleteOrEmpty) && qid.var.is_empty() && !qid.prefix.is_empty())) {
        // FIXME: Improve these error messages
        Token token = peek_token(parser);
        parser->error(Error, token.input_offset, FMT_MSG("expected qualified-id before '{}'", token.to_string()));
    }
    return qid;
}

// Consumes as much as it can; unrecognized tokens are returned to caller without logging an error:
void parse_conversion_type_id(ParserImpl* parser, QualifiedID::ConversionFunc* conv) {
    bool got_type_specifier = false;
    for (;;) {
        Token token = peek_token(parser);
        if (token.type != Token::Identifier)
            break;

        if (token.text == "const" || token.text == "volatile") {
            parser->token_index++;
            conv->decl_specifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Keyword{token}));
        } else {
            QualifiedID qid = parse_qualified_id(parser, ParseQualifiedMode::RequireComplete);
            if (got_type_specifier) {
                // We already got a type specifier.
                // This is not a breaking error; just ignore it and continue from here.
                parser->error_no_mute(Error, get_first_token(qid).input_offset, "too many type specifiers");
            } else {
                got_type_specifier = true;
                PLY_ASSERT(!qid.var.is_empty()); // Shouldn't happen because token was an identifier
                conv->decl_specifiers.append(
                    Heap::create<DeclSpecifier>(DeclSpecifier::TypeSpecifier{{}, std::move(qid)}));
            }
        }
    }

    // Parse the optional (limited) abstract declarator part:
    bool allow_qualifier = false;
    for (;;) {
        Token token = peek_token(parser);
        if (token.type == Token::Star || token.type == Token::SingleAmpersand || token.type == Token::DoubleAmpersand) {
            parser->token_index++;
            auto* prod = Heap::create<DeclProduction>();
            auto& ptr_to = prod->var.switch_to<DeclProduction::Indirection>();
            ptr_to.punc = token;
            prod->child = std::move(conv->abstract_dcor);
            conv->abstract_dcor = std::move(prod);
            allow_qualifier = (token.type == Token::Star);
        } else if (token.type == Token::Identifier) {
            if (token.text == "const" || token.text == "volatile") {
                parser->token_index++;
                if (!allow_qualifier) {
                    // Qualifier not allowed here (eg. immediately after comma in declarator
                    // list). This is not a breaking error; just ignore it and continue from here.
                    parser->error_no_mute(Error, token.input_offset,
                                          FMT_MSG("'{}' qualifier not allowed here", token.text));
                }

                auto* prod = Heap::create<DeclProduction>();
                auto& qualifier = prod->var.switch_to<DeclProduction::Qualifier>();
                qualifier.keyword = token;
                prod->child = std::move(conv->abstract_dcor);
                conv->abstract_dcor = std::move(prod);
            } else
                break;
        } else
            break;
    }
}

//  ▄▄▄▄▄                       ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀
//  ▄▄▄▄▄               ▄▄▄                        ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄
//  ██  ██ ██▄▄██ ██     ██   ▄▄▄██ ██  ▀▀  ▄▄▄██  ██   ██  ██ ██  ▀▀ ▀█▄▄▄
//  ██▄▄█▀ ▀█▄▄▄  ▀█▄▄▄ ▄██▄ ▀█▄▄██ ██     ▀█▄▄██  ▀█▄▄ ▀█▄▄█▀ ██      ▄▄▄█▀
//

Parameter parse_template_parameter(ParserImpl* parser) {
    Parameter result;
    s32 type_specifier_index = -1;
    for (;;) {
        Token token = peek_token(parser);
        if (token.type == Token::Identifier) {
            if (token.text == "const" || token.text == "volatile" || token.text == "unsigned") {
                parser->mute_errors = false;
                parser->token_index++;
                result.decl_specifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Keyword{token}));
            } else {
                if (token.text == "typename" || token.text == "class") {
                    if (type_specifier_index < 0) {
                        parser->mute_errors = false;
                    } else {
                        parser->error(Error, token.input_offset, "too many type specifiers");
                    }
                    parser->token_index++;
                    DeclSpecifier* decl_spec = result.decl_specifiers.append(Heap::create<DeclSpecifier>());
                    auto& type_param = decl_spec->var.switch_to<DeclSpecifier::TypeParameter>();
                    type_param.keyword = token;

                    Token t2 = peek_token(parser);
                    if (t2.type == Token::Ellipsis) {
                        parser->token_index++;
                        type_param.ellipsis = t2;
                    }

                    QualifiedID qid = parse_qualified_id(parser, ParseQualifiedMode::RequireCompleteOrEmpty);
                    if (!qid.prefix.is_empty()) {
                        if (token.text == "typename") {
                            // Treat this qualified name as non-type template parameter.
                            type_specifier_index = result.decl_specifiers.num_items();
                            auto& type_spec = decl_spec->var.switch_to<DeclSpecifier::TypeSpecifier>();
                            type_spec.elaborate_keyword = token;
                            type_spec.qid = std::move(qid);
                            continue;
                        } else {
                            parser->error(Error, get_first_token(qid).input_offset,
                                          "template parameter name cannot have a nested name prefix");
                        }
                    } else if (auto* ident = qid.var.as<QualifiedID::Identifier>()) {
                        result.identifier = ident->name;
                    } else if (!qid.is_empty()) {
                        parser->error(Error, get_first_token(qid).input_offset, "expected identifier");
                    }
                    parse_optional_type_id_initializer(parser, result.init);
                    return result;
                }

                parser->mute_errors = false;
                if (type_specifier_index >= 0)
                    break; // Parse it as a declarator.

                type_specifier_index = result.decl_specifiers.num_items();
                DeclSpecifier* decl_spec = result.decl_specifiers.append(Heap::create<DeclSpecifier>());
                auto& type_spec = decl_spec->var.switch_to<DeclSpecifier::TypeSpecifier>();
                type_spec.qid = parse_qualified_id(parser, ParseQualifiedMode::RequireComplete);
            }
        } else {
            // Not an identifier. We should have parsed a type specifier by now.
            if (type_specifier_index < 0) {
                parser->error(Error, token.input_offset,
                              FMT_MSG("expected template parameter before '{}'", token.to_string()));
            }
            break;
        }
    }

    Declarator dcor;
    parse_declarator(parser, dcor, nullptr, DeclaratorFlags::AllowNamed | DeclaratorFlags::AllowAbstract);
    if (!dcor.qid.is_empty()) {
        if (!dcor.qid.prefix.is_empty()) {
            parser->error(Error, get_first_token(dcor.qid).input_offset,
                          "template parameter name cannot have a nested-name prefix");
        } else if (!dcor.qid.var.is<QualifiedID::Identifier>()) {
            parser->error(Error, get_first_token(dcor.qid).input_offset, "expected identifier");
        } else {
            result.identifier = std::move(dcor.qid.var.as<QualifiedID::Identifier>()->name);
        }
    }
    result.prod = std::move(dcor.prod);
    parse_optional_variable_initializer(parser, result.init, false);
    return result;
}

Parameter parse_function_parameter(ParserImpl* parser) {
    Parameter result;
    s32 type_specifier_index = -1;
    for (;;) {
        Token token = peek_token(parser);
        if (token.type == Token::Identifier) {
            if (token.text == "const" || token.text == "volatile" || token.text == "unsigned") {
                parser->mute_errors = false;
                parser->token_index++;
                result.decl_specifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Keyword{token}));
            } else if (token.text == "typename" || token.text == "struct" || token.text == "class" ||
                       token.text == "union" || token.text == "enum") {
                if (type_specifier_index < 0) {
                    parser->mute_errors = false;
                } else {
                    parser->error(Error, token.input_offset, "too many type specifiers");
                }
                parser->token_index++;
                DeclSpecifier* decl_spec = result.decl_specifiers.append(Heap::create<DeclSpecifier>());
                auto& type_spec = decl_spec->var.switch_to<DeclSpecifier::TypeSpecifier>();
                type_spec.elaborate_keyword = token;
                type_spec.qid = parse_qualified_id(parser, ParseQualifiedMode::RequireComplete);
            } else {
                parser->mute_errors = false;
                if (type_specifier_index >= 0)
                    break; // This must be the declarator part.

                type_specifier_index = result.decl_specifiers.num_items();
                DeclSpecifier* decl_spec = result.decl_specifiers.append(Heap::create<DeclSpecifier>());
                auto& type_spec = decl_spec->var.switch_to<DeclSpecifier::TypeSpecifier>();
                type_spec.qid = parse_qualified_id(parser, ParseQualifiedMode::RequireComplete);
                // We should check at this point that qid actually refers to a type (if possible!). Consider for example
                // (inside class 'Foo'):
                //      Foo(baz())
                // If 'baz' refers to a type, it's a constructor. Otherwise, it's a function 'baz'
                // returning Foo. If it's not possible to determine in this pass, we obviously have
                // to guess (leaning towards it being a constructor), but the parse tree should
                // store the fact that we guessed somewhere.
            }
        } else {
            // Not an identifier. We should have parsed a type specifier by now.
            if (type_specifier_index < 0) {
                parser->error(Error, token.input_offset,
                              FMT_MSG("expected parameter type before '{}'", token.to_string()));
            }
            break;
        }
    }

    Declarator dcor;
    parse_declarator(parser, dcor, nullptr, DeclaratorFlags::AllowNamed | DeclaratorFlags::AllowAbstract);
    if (!dcor.qid.is_empty()) {
        if (!dcor.qid.prefix.is_empty()) {
            parser->error(Error, get_first_token(dcor.qid).input_offset,
                          "parameter name cannot have a nested-name prefix");
        } else if (!dcor.qid.var.is<QualifiedID::Identifier>()) {
            parser->error(Error, get_first_token(dcor.qid).input_offset, "expected identifier");
        } else {
            result.identifier = std::move(dcor.qid.var.as<QualifiedID::Identifier>()->name);
        }
    }
    result.prod = std::move(dcor.prod);
    parse_optional_variable_initializer(parser, result.init, false);
    return result;
}

Array<Token> parse_function_qualifier_seq(ParserImpl* parser) {
    Array<Token> qualifiers;

    // Read trailing qualifiers
    for (;;) {
        Token token = peek_token(parser);
        if (token.type == Token::Identifier && (token.text == "const" || token.text == "override")) {
            parser->token_index++;
            qualifiers.append(token);
        } else if (token.type == Token::SingleAmpersand || token.type == Token::DoubleAmpersand) {
            parser->token_index++;
            qualifiers.append(token);
        } else {
            break;
        }
    }

    return qualifiers;
}

struct ParseParams {
    Token::Type open_punc = Token::OpenParen;
    Token::Type close_punc = Token::CloseParen;

    static ParseParams Func;
    static ParseParams Template;
};

ParseParams ParseParams::Func = {};
ParseParams ParseParams::Template = {
    Token::OpenAngle,
    Token::CloseAngle,
};

void parse_parameter_declaration_list(ParserImpl* parser, Array<Parameter>* params, bool for_template) {
    const ParseParams* pp = for_template ? &ParseParams::Template : &ParseParams::Func;

    parser->mute_errors = false;

    Token token = peek_token(parser);
    if (token.type == pp->close_punc)
        return; // Empty parameter declaration list

    SetAcceptFlagsInScope accept_scope{parser, pp->open_punc};

    for (;;) {
        // A parameter declaration is expected here.
        Parameter* param = nullptr;
        bool any_tokens_consumed = false;

        Token expected_loc = peek_token(parser);
        if (expected_loc.type == Token::Ellipsis && !for_template) {
            parser->token_index++;
            // FIXME: Check somewhere that this is the last parameter
            param = &params->append();
            DeclSpecifier* decl_spec = Heap::create<DeclSpecifier>();
            auto& ellipsis = decl_spec->var.switch_to<DeclSpecifier::Ellipsis>();
            ellipsis.token = expected_loc;
            param->decl_specifiers.append(decl_spec);
            any_tokens_consumed = true;
        } else {
            u32 saved_token_index = parser->token_index;
            if (for_template) {
                param = &params->append(parse_template_parameter(parser));
            } else {
                param = &params->append(parse_function_parameter(parser));
            }
            any_tokens_consumed = (saved_token_index != parser->token_index);
        }

        token = peek_token(parser);
        if (token.type == pp->close_punc) {
            // End of parameter declaration list
            break;
        } else if (token.type == Token::Comma) {
            parser->token_index++;
            if (param) {
                param->comma = token;
            }
        } else {
            // Unexpected token
            parser->error(Error, token.input_offset,
                          FMT_MSG("expected ',' or '{}' before '{}'", (for_template ? '>' : ')'), token.to_string()));
            parser->token_index++;
            if (any_tokens_consumed) {
                if (!handle_unexpected_token(parser, nullptr, token))
                    break;
            } else {
                if (!ok_to_stay_in_scope(parser, token))
                    break;
            }
        }
    }
}

DeclProduction* parse_parameter_list(ParserImpl* parser, Owned<DeclProduction>** prod_to_modify) {
    Token open_paren = peek_token(parser);
    if (open_paren.type != Token::OpenParen) {
        // Currently, we only hit this case when optimistically trying to parse a constructor
        PLY_ASSERT(parser->restore_point_enabled); // Just a sanity check
        parser->error(Error, open_paren.input_offset, FMT_MSG("expected '(' before '{}'", open_paren.to_string()));
        return nullptr;
    }
    parser->mute_errors = false;

    auto* prod = Heap::create<DeclProduction>();
    auto& func = prod->var.switch_to<DeclProduction::Function>();
    func.open_paren = open_paren;
    parser->token_index++;
    prod->child = std::move(**prod_to_modify);
    **prod_to_modify = prod;
    *prod_to_modify = &prod->child;

    parse_parameter_declaration_list(parser, &func.params, false);
    Token close_paren = peek_token(parser);
    if (close_paren.type == Token::CloseParen) {
        func.close_paren = close_paren;
        parser->token_index++;
        func.qualifiers = parse_function_qualifier_seq(parser);
    }
    return prod;
}

void parse_optional_trailing_return_type(ParserImpl* parser, DeclProduction* fn_prod) {
    PLY_ASSERT(fn_prod);
    PLY_ASSERT(fn_prod->var.is<DeclProduction::Function>());
    auto& function = *fn_prod->var.as<DeclProduction::Function>();

    Token arrow_token = peek_token(parser);
    if (arrow_token.type == Token::Arrow) {
        parser->token_index++;
        function.arrow = arrow_token;
        // FIXME: Should parse a TypeID here, not just a qualified ID:
        function.trailing_ret_type = parse_type_id(parser);
    }
}

// When bad tokens are encountered, it consumes them until it encounters a token that an outer scope
// is expected to handle, as determined by parser->outer_accept_flags. In that case, it returns
// early. If the bad token is one of { ( or [, it calls skip_any_scope().
//
// The first bad token sets parser->mute_errors to true. mute_errors remains true until it reaches
// the next good token. mute_errors may remain true when we return; this can happen, for example,
// when } is encountered, causing us to return early.
void parse_declarator(ParserImpl* parser, Declarator& dcor, DeclProduction* nested, u32 dcor_flags) {
    dcor.prod = nested;
    bool allow_qualifier = false;
    Owned<DeclProduction>* prod_to_modify = nullptr; // Used in phase two
    bool expecting_qualified_id = false;

    // This is the first phase of parsing a declarator. It handles everything up to trailing
    // function parameter lists and array subscripts.
    //
    // As it reads pointer, reference symbols and cv-qualifiers, it inserts new
    // DeclaratorProductions at the *head* of the current DeclarationProduction chain
    // (dcor.prod) so that they are effectively read right-to-left. For example,
    //      * const &
    // becomes "reference to const pointer" in the DeclarationProduction chain.
    //
    // Pointers can also have nested name specifiers, making them pointer-to-members:
    //      Foo::*
    //
    // If an open parenthesis is encountered during this phase, and the Allow_Abstract flags is
    // set, it first tries to parse a function parameter list; otherwise, or if that fails, it
    // tries to parse a nested declarator. If it's a nested declarator, nested
    // DeclarationProductions are inserted at the head of the current DeclarationProduction
    // chain. In either case, no further pointer/reference/cv-qualifiers are expected after the
    // closing parenthesis, so we break out of the loop and proceed to the second phase.
    //
    // When a qualified ID is encountered, it's considered the name of the declarator (in other
    // words, the declarator is not abstract), and we break out of the loop and proceed to the
    // second phase.

    for (;;) {
        // Try to tokenize a qualified ID.
        QualifiedID qid = parse_qualified_id(parser, ParseQualifiedMode::AllowIncomplete);
        if (!qid.var.is_empty()) {
            if ((dcor_flags & DeclaratorFlags::AllowNamed) == 0) {
                // Qualified ID is not allowed here
                // FIXME: Should rewind instead of consuming the qualified-id????
                // The caller may log a more informative error at this token! (check test suite)
                parser->error_no_mute(Error, get_first_token(qid).input_offset, "type-id cannot have a name");
                // Don't mute errors
            }
            dcor.qid = std::move(qid);
            break; // Got qualified-id
        }
        // qid.unqual is empty, but qid.prefix might be a pointer prefix (as in a
        // pointer-to-member).

        Token token = read_token(parser);
        if (token.type == Token::OpenParen) {
            if (!qid.prefix.is_empty()) {
                // Should not be preceded by nested name specifier
                parser->error_no_mute(Error, token.input_offset,
                                      FMT_MSG("'{}' cannot have a nested name prefix", token.to_string()));
                // Don't mute errors
            }

            parser->mute_errors = false;

            if ((dcor_flags & DeclaratorFlags::AllowAbstract) != 0) {
                // If abstract declarators are allowed, try to parse a function parameter list
                // first.
                parser->token_index--;
                RestorePoint rp{parser};
                // FIXME: When a restore point is active, handle_unexpected_token() should always
                // return false. Otherwise, parse_parameter_list could end up consuming way too many
                // tokens, and it might even incorrectly "pre-tokenize" '>>' as a right-shift
                // operator instead of as two CloseAngles...
                DeclProduction* saved_prod = dcor.prod;
                prod_to_modify = &dcor.prod;
                DeclProduction* fn_prod = parse_parameter_list(parser, &prod_to_modify);
                if (!rp.error_occurred()) {
                    // Success. Parse optional trailing return type. If any parse errors occur while
                    // doing so, we won't backtrack.
                    PLY_ASSERT(fn_prod);
                    rp.cancel();
                    parse_optional_trailing_return_type(parser, fn_prod);
                    // Break out of the loop and continue with the second phase.
                    break;
                }

                // It didn't parse as a function parameter list.
                // Roll back any productions that were created:
                while (dcor.prod != saved_prod) {
                    PLY_ASSERT(dcor.prod);
                    DeclProduction* child = dcor.prod->child.release();
                    dcor.prod = child;
                }
                rp.backtrack();
                rp.cancel();
                token = read_token(parser);
                prod_to_modify = nullptr;
            }

            // Parse it as a nested declarator.
            Declarator target;
            parse_declarator(parser, target, dcor.prod.release(), dcor_flags);
            dcor.prod = Heap::create<DeclProduction>();
            auto& parenthesized = dcor.prod->var.switch_to<DeclProduction::Parenthesized>();
            parenthesized.open_paren = token;
            dcor.prod->child = std::move(target.prod);
            PLY_ASSERT(dcor.qid.is_empty());
            dcor.qid = std::move(target.qid);

            if (!close_scope(parser, &parenthesized.close_paren, token))
                return;
            break;
        }

        if (!qid.prefix.is_empty()) {
            if (token.type != Token::Star) {
                // Should not be preceded by nested name specifier
                parser->error_no_mute(Error, token.input_offset,
                                      FMT_MSG("'{}' cannot have a nested name prefix", token.to_string()));
            }
        }

        if (token.type == Token::Star || token.type == Token::SingleAmpersand || token.type == Token::DoubleAmpersand) {
            parser->mute_errors = false;

            auto* prod = Heap::create<DeclProduction>();
            auto& ptr_to = prod->var.switch_to<DeclProduction::Indirection>();
            ptr_to.prefix = std::move(qid.prefix);
            ptr_to.punc = token;
            prod->child = std::move(dcor.prod);
            dcor.prod = prod;
            allow_qualifier = (token.type == Token::Star);
        } else if (token.type == Token::Ellipsis) {
            // FIXME: Make a Production rule for this

            parser->mute_errors = false;
        } else if (token.type == Token::Identifier) {
            PLY_ASSERT(qid.prefix.is_empty());
            PLY_ASSERT(token.text == "const" || token.text == "volatile" || token.text == "inline" ||
                       token.text == "static" || token.text == "friend");
            if (!allow_qualifier) {
                // Qualifier not allowed here
                parser->error_no_mute(Error, token.input_offset,
                                      FMT_MSG("'{}' qualifier not allowed here", token.text));
                // Handle it anyway...
            }

            parser->mute_errors = false;

            auto* prod = Heap::create<DeclProduction>();
            auto& qualifier = prod->var.switch_to<DeclProduction::Qualifier>();
            qualifier.keyword = token;
            prod->child = std::move(dcor.prod);
            dcor.prod = prod;
        } else {
            // End of first phase of parsing a declarator.
            PLY_ASSERT(qid.prefix.is_empty());
            if ((dcor_flags & DeclaratorFlags::AllowAbstract) == 0) {
                // Note that we still allow "empty" declarators (in other words, abstract
                // declarators with no DeclaratorProductions) even when Allow_Abstract is not
                // specified, so that class definitions like:
                //      struct Foo {};
                // do not log an error.
                //
                // With this in mind, if a declarator name was required but
                // none was given, log an error *only if* some DeclaratorProductions have been
                // created.
                //
                // FIXME: Log an error (or warning?) if it's an empty declarators that *doesn't*
                // define a new class/struct/union, such as:
                //      int;
                if (dcor.prod) {
                    parser->error(Error, token.input_offset,
                                  FMT_MSG("expected qualified-id before '{}'", token.to_string()));
                } else {
                    // No DeclaratorProductions have been created yet. We'll log an error if any are
                    // created in the second phase.
                    expecting_qualified_id = true;
                }
            }
            parser->token_index--;
            break;
        }
    }

    // This is the second phase of parsing a declarator. It parses only trailing function
    // parameter lists and array subscripts. A subchain of DeclaratorProductions is built in the
    // same order that these are encountered, so that they're effectively read left-to-right.
    // For example,
    //      []()
    // becomes "array of functions" in the subchain. This subchain is inserted at the head of
    // dcor.prod, the current Decl_Production chain being built.
    //
    // Note that this phase can take place inside a nested declarator, which means that the
    // caller may continue inserting DeclaratorProductions at the head of the chain after we
    // return.
    //
    // FIXME: make sure this approach works correctly for things like (*x())()

    if (!prod_to_modify) {
        prod_to_modify = &dcor.prod;
    }
    for (;;) {
        Token token = peek_token(parser);
        auto check_expecting_qualified_id = [&]() {
            parser->mute_errors = false;
            if (expecting_qualified_id) {
                parser->error(Error, token.input_offset,
                              FMT_MSG("expected qualified-id before '{}'", token.to_string()));
                expecting_qualified_id = false;
            }
        };

        if (token.type == Token::OpenSquare) {
            parser->token_index++;
            check_expecting_qualified_id();

            auto* prod = Heap::create<DeclProduction>();
            auto& array_of = prod->var.switch_to<DeclProduction::ArrayOf>();
            array_of.open_square = token;
            prod->child = std::move(*prod_to_modify);
            *prod_to_modify = prod;
            prod_to_modify = &prod->child;

            parse_expression(parser, true);

            if (!close_scope(parser, &array_of.close_square, token))
                return;
        } else if (token.type == Token::OpenParen) {
            check_expecting_qualified_id();

            DeclProduction* fn_prod = parse_parameter_list(parser, &prod_to_modify);
            if (fn_prod) {
                parse_optional_trailing_return_type(parser, fn_prod);
            }
        } else
            break;
    }
}

//  ▄▄▄▄▄                       ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀
//  ▄▄▄▄        ▄▄  ▄▄   ▄▄        ▄▄▄  ▄▄
//   ██  ▄▄▄▄▄  ▄▄ ▄██▄▄ ▄▄  ▄▄▄▄   ██  ▄▄ ▄▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄
//   ██  ██  ██ ██  ██   ██  ▄▄▄██  ██  ██   ▄█▀  ██▄▄██ ██  ▀▀ ▀█▄▄▄
//  ▄██▄ ██  ██ ██  ▀█▄▄ ██ ▀█▄▄██ ▄██▄ ██ ▄██▄▄▄ ▀█▄▄▄  ██      ▄▄▄█▀
//

void skip_member_initializer_list(ParserImpl* parser) {
    // Make sure that if { is encountered (even with unexpected placement), we return to caller.
    PLY_SET_IN_SCOPE(parser->outer_accept_flags, parser->outer_accept_flags | ParserImpl::AcceptOpenCurly);
    // FIXME: Add a scope to declare that we are parsing a member initializer list, and report this
    // scope in any logged errors (?)

    for (;;) {
        QualifiedID qid = parse_qualified_id(parser, ParseQualifiedMode::AllowIncomplete);
        if (!qid.var.is_empty()) {
            Token open_brace_token = peek_token(parser);
            if ((open_brace_token.type == Token::OpenParen) || (open_brace_token.type == Token::OpenCurly)) {
                parser->token_index++;
                skip_any_scope(parser, nullptr, open_brace_token);
            } else {
                // expected ( or {
                // FIXME: should report that it was expected after qualified id
                parser->error(Error, open_brace_token.input_offset,
                              FMT_MSG("expected '{{' or '(' before '{}'", open_brace_token.to_string()));
                continue;
            }

            Token next_token = peek_token(parser);
            if (next_token.type == Token::OpenCurly) {
                // End of member initializer list.
                parser->mute_errors = false;
                break;
            } else if (next_token.type == Token::Comma) {
                parser->token_index++;
                parser->mute_errors = false;
            } else {
                parser->error(Error, next_token.input_offset, "expected function body after member initializer list");
                break;
            }
        } else {
            Token token = peek_token(parser);
            parser->error(Error, token.input_offset,
                          FMT_MSG("expected class member or base class name before '{}'", token.to_string()));
            if (qid.prefix.is_empty()) {
                parser->token_index++;
                if (!handle_unexpected_token(parser, nullptr, token))
                    break;
            }
        }
    }
}

void parse_optional_function_body(ParserImpl* parser, Initializer& result, const Declaration::Entity& entity) {
    result.var = {};
    Token token = peek_token(parser);
    if (token.type == Token::SingleEqual) {
        parser->token_index++;
        auto& assign = result.var.switch_to<Initializer::Assignment>();
        assign.equal_sign = token;
        parse_expression(parser); // FIXME: Fill in var_init
        return;
    }
    if (token.type == Token::SingleColon) {
        parser->token_index++;
        auto& func_body = result.var.switch_to<Initializer::FunctionBody>();
        func_body.colon = token;
        // FIXME: populate MemberInitializer
        skip_member_initializer_list(parser);
        token = peek_token(parser);
    }
    if (token.type == Token::OpenCurly) {
        parser->token_index++;
        auto& func_body = result.var.switch_to<Initializer::FunctionBody>();
        func_body.colon = token;
        skip_any_scope(parser, &func_body.close_curly, token);
    }
}

void parse_optional_type_id_initializer(ParserImpl* parser, Initializer& result) {
    result.var = {};
    Token token = peek_token(parser);
    if (token.type == Token::SingleEqual) {
        parser->token_index++;
        auto& assign = result.var.switch_to<Initializer::Assignment>();
        assign.equal_sign = token;
        token = read_token(parser);
        if (token.text == "0") {
            // FIXME: Support <typename A::B = 0> correctly!
        } else {
            parser->token_index--;
            u32 saved_error_count = parser->raw_error_count;
            TypeID type_id = parse_type_id(parser);
            if (saved_error_count == parser->raw_error_count) {
                // No errors
                assign.var = std::move(type_id);
            }
        }
    }
}

void parse_optional_variable_initializer(ParserImpl* parser, Initializer& result, bool allow_braced_init) {
    PLY_ASSERT(result.var.is_empty());
    Token token = peek_token(parser);
    if (token.type == Token::OpenCurly) {
        // It's a variable initializer
        result.var.switch_to<Initializer::Assignment>();
        parse_expression(parser); // FIXME: Fill in var_init
    } else if (token.type == Token::SingleEqual) {
        parser->token_index++;
        auto& assign = result.var.switch_to<Initializer::Assignment>();
        assign.equal_sign = token;
        parse_expression(parser);
        assign.var.switch_to<Owned<Expression>>();
        // FIXME: Fill in
    } else if (token.type == Token::SingleColon) {
        parser->token_index++;
        auto& bit_field = result.var.switch_to<Initializer::BitField>();
        bit_field.colon = token;
        parse_expression(parser);
    }
}

void parse_init_declarators(ParserImpl* parser, Declaration::Entity& entity) {
    // A list of zero or more named declarators is accepted here.
    for (;;) {
        Declarator dcor;
        parse_declarator(parser, dcor, nullptr, DeclaratorFlags::AllowNamed);
        if (dcor.qid.is_empty())
            break; // Error was already logged
        InitDeclarator& init_dcor = entity.init_declarators.append();
        init_dcor.qid = std::move(dcor.qid);
        init_dcor.prod = std::move(dcor.prod);
        if (init_dcor.prod && init_dcor.prod->var.is<DeclProduction::Function>()) {
            parse_optional_function_body(parser, init_dcor.init, entity);
            if (init_dcor.init.var.is<Initializer::FunctionBody>()) {
                if (entity.init_declarators.num_items() > 1) {
                    // Note: Mixing function definitions and declarations could be a
                    // higher-level error instead of a parse error.
                    // FIXME: A reference to both declarators should be part of the error
                    // message. For now, we'll just use the open parenthesis token.
                    parser->error_no_mute(Error,
                                          init_dcor.prod->var.as<DeclProduction::Function>()->open_paren.input_offset,
                                          "can't mix function definitions with other declarations");
                }
            }
            break; // Stop parsing declarators immediately after the function body.
        } else {
            parse_optional_variable_initializer(parser, init_dcor.init, true);
        }
        Token sep_token = peek_token(parser);
        if (sep_token.type == Token::Comma) {
            parser->token_index++;
            if (init_dcor.init.var.is<Initializer::FunctionBody>()) {
                // FIXME: It's not very clear from this error message that the comma is the
                // token that triggered an error. In any case, we don't hit this codepath yet,
                // as explained by the above comment.
                PLY_ASSERT(0); // codepath never gets hit at the moment
                parser->error_no_mute(Error,
                                      init_dcor.prod->var.as<DeclProduction::Function>()->open_paren.input_offset,
                                      "can't mix function definitions with other declarations");
            }
            init_dcor.comma = sep_token;
        } else
            break;
    }
}

//  ▄▄▄▄▄                       ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀
//  ▄▄▄▄▄               ▄▄▄                        ▄▄   ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄██▄▄ ▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄
//  ██  ██ ██▄▄██ ██     ██   ▄▄▄██ ██  ▀▀  ▄▄▄██  ██   ██ ██  ██ ██  ██ ▀█▄▄▄
//  ██▄▄█▀ ▀█▄▄▄  ▀█▄▄▄ ▄██▄ ▀█▄▄██ ██     ▀█▄▄██  ▀█▄▄ ██ ▀█▄▄█▀ ██  ██  ▄▄▄█▀
//

Array<DeclSpecifier::Class::BaseSpecifier> parse_base_specifier_list(ParserImpl* parser) {
    Array<DeclSpecifier::Class::BaseSpecifier> base_specifiers;
    for (;;) {
        DeclSpecifier::Class::BaseSpecifier base_spec;

        // Optional access specifier
        Token token = peek_token(parser);
        if (token.type == Token::Identifier) {
            if (token.text == "public" || token.text == "private" || token.text == "protected") {
                parser->token_index++;
                parser->mute_errors = false;
                base_spec.access_spec = token;
                token = peek_token(parser);
            }
        }

        // Qualified ID
        base_spec.base_qid = parse_qualified_id(parser, ParseQualifiedMode::RequireComplete);
        if (base_spec.base_qid.var.is_empty())
            break;
        parser->mute_errors = false;
        DeclSpecifier::Class::BaseSpecifier& added_bs = base_specifiers.append(std::move(base_spec));

        // Comma or {
        Token punc_token = peek_token(parser);
        if (punc_token.type == Token::OpenCurly)
            break;
        if (punc_token.type == Token::Comma) {
            parser->token_index++;
            added_bs.comma = token;
        } else {
            parser->token_index++;
            parser->error(Error, punc_token.input_offset,
                          FMT_MSG("expected ',' or '{{' before '{}'", punc_token.to_string()));
            // FIXME: Call handle_unexpected_token
            break;
        }
    }
    return base_specifiers;
}

DeclSpecifier::Class parse_class_declaration(ParserImpl* parser) {
    DeclSpecifier::Class class_;
    Token token = read_token(parser);
    class_.keyword = token;
    class_.qid = parse_qualified_id(parser, ParseQualifiedMode::RequireCompleteOrEmpty);

    // Read optional virt-specifier sequence
    {
        Token final_tok;
        for (;;) {
            token = read_token(parser);
            if (token.text == "final") {
                if (final_tok.is_valid()) {
                    parser->error(Error, token.input_offset, FMT_MSG("'{}' used more than once", token.text));
                } else {
                    final_tok = token;
                    class_.virt_specifiers.append(token);
                }
            } else {
                break;
            }
        }
    }

    if (token.type == Token::SingleColon) {
        class_.colon = token;
        class_.base_specifiers = parse_base_specifier_list(parser);
        token = read_token(parser);
    }

    if (token.type == Token::OpenCurly) {
        class_.open_curly = token;
        class_.member_decls = parse_declaration_list(parser, &class_.close_curly, get_class_name(class_.qid));
    } else {
        parser->token_index--;
    }
    return class_;
}

void parse_enum_body(ParserImpl* parser, DeclSpecifier::Enum* en) {
    parser->mute_errors = false;
    SetAcceptFlagsInScope accept_scope{parser, Token::OpenCurly};

    for (;;) {
        Token token = read_token(parser);
        if (token.type == Token::CloseCurly) {
            // Done
            parser->mute_errors = false;
            en->close_curly = token;
            break;
        } else if (token.type == Token::Identifier) {
            parser->mute_errors = false;

            // Create enor
            DeclSpecifier::Enum::Item& enor = en->enumerators.append();
            enor.text = token;
            parse_optional_variable_initializer(parser, enor.init, false);
            Token token2 = read_token(parser);
            bool done = false;
            if (token2.type == Token::Comma) {
                parser->mute_errors = false;
                enor.comma = token2;
            } else if (token2.type == Token::CloseCurly) {
                // Done
                parser->mute_errors = false;
                en->close_curly = token2;
                done = true;
            } else {
                // expected , or } after enum member
                if (token2.type == Token::Identifier) {
                    parser->error(Error, token2.input_offset, "missing ',' between enumerators");
                }
                // Other tokens will generate an error on next loop iteration
                parser->token_index--;
            }
            if (done)
                break;
        } else {
            // expected enumerator or }
            parser->error(Error, token.input_offset,
                          FMT_MSG("expected enumerator or '}}' before '{}'", token.to_string()));
            if (!handle_unexpected_token(parser, nullptr, token))
                return;
        }
    }
}

DeclSpecifier::Enum parse_enum_declaration(ParserImpl* parser) {
    DeclSpecifier::Enum en;
    en.keyword = read_token(parser);
    Token token2 = peek_token(parser);
    if ((token2.type == Token::Identifier) && (token2.text == "class")) {
        parser->token_index++;
        en.class_keyword = token2;
    }

    en.qid = parse_qualified_id(parser, ParseQualifiedMode::RequireCompleteOrEmpty);

    Token sep_token = peek_token(parser);
    if (sep_token.type == Token::SingleColon) {
        parser->token_index++;
        if (en.qid.is_empty()) {
            parser->error_no_mute(Error, sep_token.input_offset, "scoped enum requires a name");
        }
        en.colon = sep_token;
        en.base = parse_qualified_id(parser, ParseQualifiedMode::RequireComplete);
    }

    Token token3 = peek_token(parser);
    if (token3.type == Token::OpenCurly) {
        parser->token_index++;
        en.open_curly = token3;
        parse_enum_body(parser, &en);
    }
    return en;
}

bool looks_like_ctor_dtor(StringView enclosing_class_name, const QualifiedID& qid) {
    if (enclosing_class_name.is_empty()) {
        if (qid.prefix.num_items() < 1)
            return false;

        StringView ctor_dtor_name = get_ctor_dtor_name(qid);
        if (ctor_dtor_name.is_empty())
            return false;

        const QualifiedID::Prefix& tail = qid.prefix.back();
        if (auto* ident = tail.var.as<QualifiedID::Identifier>()) {
            PLY_ASSERT(ident->name.is_valid());
            return ctor_dtor_name == ident->name.text;
        } else if (auto* tmpl_id = tail.var.as<QualifiedID::TemplateID>()) {
            PLY_ASSERT(tmpl_id->name.is_valid());
            return ctor_dtor_name == tmpl_id->name.text;
        }

        return false;
    } else {
        if (qid.prefix.num_items() > 0)
            return false;

        StringView ctor_dtor_name = get_ctor_dtor_name(qid);
        return ctor_dtor_name == enclosing_class_name;
    }
}

Declaration parse_entity_declaration(ParserImpl* parser, StringView enclosing_class_name) {
    Declaration result;
    auto& entity = result.var.switch_to<Declaration::Entity>();
    u32 start_input_offset = peek_token(parser).input_offset;
    u32 saved_error_count = parser->raw_error_count;

    // Parse the decl-specifier sequence.
    s32 type_specifier_index = -1;
    for (;;) {
        Token token = peek_token(parser);
        if (token.type == Token::Identifier) {
            if (token.text == "extern") {
                parser->mute_errors = false;
                parser->token_index++;
                Token literal = peek_token(parser);
                if (literal.type == Token::StringLiteral) {
                    parser->token_index++;
                    entity.decl_specifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Linkage{token, literal}));
                } else {
                    entity.decl_specifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Keyword{token}));
                }
            } else if (token.text == "inline" || token.text == "const" || token.text == "volatile" ||
                       token.text == "static" || token.text == "friend" || token.text == "virtual" ||
                       token.text == "constexpr" || token.text == "thread_local" || token.text == "unsigned" ||
                       token.text == "mutable" || token.text == "explicit") {
                parser->mute_errors = false;
                parser->token_index++;
                entity.decl_specifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Keyword{token}));
            } else if (token.text == "alignas") {
                parser->mute_errors = false;
                parser->token_index++;
                // FIXME: Implement Decl_Specifier::AlignAs
                // Note: alignas is technically part of the attribute-specifier-seq in the
                // grammar, which means it can only appear before the decl-specifier-seq. But
                // for now, let's just accept it here:
                Token open_paren = read_token(parser);
                if (open_paren.type != Token::OpenParen) {
                    parser->error(Error, open_paren.input_offset,
                                  FMT_MSG("expected '(' before '{}'", open_paren.to_string()));
                    continue;
                }
                // FIXME: Accept integral constant expression here too
                TypeID type_id = parse_type_id(parser);
                Token close_paren;
                if (!close_scope(parser, &close_paren, open_paren))
                    break;
            } else if (token.text == "typedef") {
                parser->mute_errors = false;
                parser->token_index++;
                // FIXME: Store this token in the parse tree
            } else if (token.text == "struct" || token.text == "class" || token.text == "union") {
                parser->mute_errors = false;
                // FIXME: for TemplateParams, "class" should be treated like "typename".
                // Otherwise, it seems C++20 may actually support structs as non-type template
                // parameters, so we should revisit this eventually.
                if (type_specifier_index >= 0) {
                    // Already got type specifier
                    parser->error(Error, token.input_offset, "too many type specifiers");
                }
                DeclSpecifier::Class class_ = parse_class_declaration(parser);
                type_specifier_index = entity.decl_specifiers.num_items();
                entity.decl_specifiers.append(Heap::create<DeclSpecifier>(std::move(class_)));
            } else if (token.text == "enum") {
                parser->mute_errors = false;
                if (type_specifier_index >= 0) {
                    parser->error(Error, token.input_offset, "too many type specifiers");
                }
                DeclSpecifier::Enum en = parse_enum_declaration(parser);
                type_specifier_index = entity.decl_specifiers.num_items();
                entity.decl_specifiers.append(Heap::create<DeclSpecifier>(std::move(en)));
            } else if ((token.text == "operator") && (type_specifier_index < 0)) {
                parser->mute_errors = false;
                parser->token_index++;
                // It's a conversion function
                InitDeclarator& init_dcor = entity.init_declarators.append();
                auto& conv_func = init_dcor.qid.var.switch_to<QualifiedID::ConversionFunc>();
                conv_func.operator_keyword = token;
                parse_conversion_type_id(parser, &conv_func);
                // Ensure there's an open parenthesis
                Token open_paren = peek_token(parser);
                if (open_paren.type == Token::OpenParen) {
                    parser->token_index++;
                    init_dcor.prod = Heap::create<DeclProduction>();
                    auto& func = init_dcor.prod->var.switch_to<DeclProduction::Function>();
                    func.open_paren = open_paren;
                    parse_parameter_declaration_list(parser, &func.params, false);
                    Token close_paren = peek_token(parser);
                    if (close_paren.type == Token::CloseParen) {
                        parser->token_index++;
                        func.close_paren = close_paren;
                        func.qualifiers = parse_function_qualifier_seq(parser);
                        parse_optional_function_body(parser, init_dcor.init, entity);
                    }
                    return result;
                } else {
                    parser->error(Error, open_paren.input_offset,
                                  FMT_MSG("expected '(' before '{}'", open_paren.to_string()));
                }
                break;
            } else {
                parser->mute_errors = false;
                if (type_specifier_index >= 0)
                    // We already got a type specifier, so this must be the declarator part.
                    break;

                parser->token_index++;
                Token typename_;
                QualifiedID qid;
                if (token.text == "typename") {
                    typename_ = token;
                    Token ellipsis;
                    Token t2 = peek_token(parser);
                    if (t2.type == Token::Ellipsis) {
                        parser->token_index++;
                        ellipsis = t2;
                    }
                    qid = parse_qualified_id(parser, ParseQualifiedMode::RequireCompleteOrEmpty);
                    if (ellipsis.is_valid()) {
                        parser->error(Error, ellipsis.input_offset,
                                      FMT_MSG("expected qualified-id before '{}'", ellipsis.to_string()));
                    }
                } else {
                    parser->token_index--;
                    qid = parse_qualified_id(parser, ParseQualifiedMode::RequireComplete);
                    PLY_ASSERT(!qid.is_empty()); // Shouldn't happen because token was an Identifier
                }

                if (!typename_.is_valid() && looks_like_ctor_dtor(enclosing_class_name, qid)) {
                    // Try (optimistically) to parse it as a constructor.
                    // We need a restore point in order to recover from Foo(bar())
                    RestorePoint rp{parser};
                    Declarator ctor_dcor;
                    Owned<DeclProduction>* prod_to_modify = &ctor_dcor.prod;
                    parse_parameter_list(parser, &prod_to_modify);
                    if (!rp.error_occurred()) {
                        // It's a constructor
                        PLY_ASSERT(ctor_dcor.prod && ctor_dcor.prod->var.is<DeclProduction::Function>());
                        rp.cancel();
                        InitDeclarator& init_dcor = entity.init_declarators.append();
                        init_dcor.prod = std::move(ctor_dcor.prod);
                        PLY_ASSERT(ctor_dcor.qid.is_empty());
                        init_dcor.qid = std::move(qid);
                        parse_optional_function_body(parser, init_dcor.init, entity);
                        return result;
                    }
                    // It failed to parse as a constructor. Treat this token as part of a
                    // entity type specifier instead.
                    rp.backtrack();
                }

                // In C++, all declarations must be explicitly typed; there is no "default
                // int". Therefore, this must be a entity type specifier.
                if (typename_.is_valid() && qid.prefix.is_empty()) {
                    Token first_token = get_first_token(qid);
                    parser->error(Error, first_token.input_offset,
                                  FMT_MSG("expected nested name prefix before '{}'", first_token.to_string()));
                }

                type_specifier_index = entity.decl_specifiers.num_items();
                DeclSpecifier* decl_spec = entity.decl_specifiers.append(Heap::create<DeclSpecifier>());
                auto& type_spec = decl_spec->var.switch_to<DeclSpecifier::TypeSpecifier>();
                type_spec.elaborate_keyword = typename_;
                type_spec.qid = std::move(qid);
            }
        } else {
            // Not an identifier. Parse the remainder as a declarator list (eg. may start with * or &).
            // Don't log an error if no type specifier was encountered yet, because the declarator may name a
            // destructor.
            break;
        }
    }

    // Parse init-declarators.
    parse_init_declarators(parser, entity);

    bool is_type_declaration = false;
    for (const DeclSpecifier* decl_spec : entity.decl_specifiers) {
        if (decl_spec->var.is<DeclSpecifier::Class>() || decl_spec->var.is<DeclSpecifier::Enum>()) {
            is_type_declaration = true;
            break;
        }
    }
    if ((saved_error_count == parser->raw_error_count) && entity.init_declarators.is_empty() && !is_type_declaration) {
        parser->error_no_mute(Error, start_input_offset, "declaration does not declare anything");
    }

    return result;
}

// Returns false if no input was read.
Declaration parse_declaration_internal(ParserImpl* parser, StringView enclosing_class_name) {
    Declaration result;
    Token token = peek_token(parser);

    if (token.type == Token::Identifier) {
        if (token.text == "extern") {
            // Possible linkage specification
            parser->mute_errors = false;
            RestorePoint rp{parser};

            Token token2 = read_token(parser);
            if (token2.type != Token::StringLiteral) {
                rp.backtrack();
                parse_entity_declaration(parser, enclosing_class_name);
            } else {
                Token token3 = read_token(parser);
                if (token3.type == Token::OpenCurly) {
                    // It's a linkage specification block, such as
                    //      extern "C" {
                    //          ...
                    //      }
                    rp.cancel();
                    auto& linkage = result.var.switch_to<Declaration::Linkage>();
                    linkage.extern_keyword = token;
                    linkage.literal = token2;
                    linkage.open_curly = token3;
                    linkage.child_decls = parse_declaration_list(parser, &linkage.close_curly, {});
                } else {
                    // It's a linkage specifier for the current declaration, such as
                    //      extern "C" void foo();
                    //      ^^^^^^^^^^
                    // FIXME: Make Declaration type for this
                    rp.backtrack();
                    parse_entity_declaration(parser, enclosing_class_name);
                }
            }
        } else if (token.text == "public" || token.text == "private" || token.text == "protected") {
            // Access specifier
            parser->token_index++;
            parser->mute_errors = false;
            Token punc_token = peek_token(parser);
            if (punc_token.type == Token::SingleColon) {
                parser->token_index++;
                auto& access_spec = result.var.switch_to<Declaration::AccessSpecifier>();
                access_spec.keyword = token;
                access_spec.colon = punc_token;
            } else {
                // expected :
                parser->error(Error, punc_token.input_offset,
                              FMT_MSG("expected ':' before '{}'", punc_token.to_string()));
            }
        } else if (token.text == "static_assert") {
            // static_assert
            parser->token_index++;
            parser->mute_errors = false;
            Token punc_token = peek_token(parser);
            if (punc_token.type != Token::OpenParen) {
                // expected (
                parser->error(Error, punc_token.input_offset,
                              FMT_MSG("expected '(' before '{}'", punc_token.to_string()));
            } else {
                parser->token_index++;
                Token close_token;
                bool continue_normally = skip_any_scope(parser, &close_token, punc_token);
                if (continue_normally) {
                    auto& sa = result.var.switch_to<Declaration::StaticAssert>();
                    sa.keyword = token;
                    sa.open_paren = punc_token;
                    sa.close_paren = close_token;
                }
            }
        } else if (token.text == "namespace") {
            // namespace
            parser->token_index++;
            parser->mute_errors = false;
            auto& ns = result.var.switch_to<Declaration::Namespace>();
            ns.keyword = token;

            Token token = peek_token(parser);
            if (token.type == Token::Identifier) {
                // FIXME: Ensure it's not a reserved word
                ns.qid = parse_qualified_id(parser, ParseQualifiedMode::RequireComplete);
                token = peek_token(parser);
            }

            if (token.type == Token::OpenCurly) {
                parser->token_index++;
                ns.open_curly = token;
                ns.child_decls = parse_declaration_list(parser, &ns.close_curly, {});
            } else {
                // expected {
                parser->error(Error, token.input_offset, FMT_MSG("expected '{{' before '{}'", token.to_string()));
            }
        } else if (token.text == "template") {
            // template
            parser->token_index++;
            parser->mute_errors = false;
            auto& tmpl = result.var.switch_to<Declaration::Template>();
            tmpl.keyword = token;
            Token token2 = peek_token(parser);
            if (token2.type == Token::OpenAngle) {
                tmpl.open_angle = token2;
                parser->token_index++;
                PLY_SET_IN_SCOPE(parser->tkr.config.tokenize_right_shift, false);
                parse_parameter_declaration_list(parser, &tmpl.params, true);
                Token close_angle = peek_token(parser);
                if (close_angle.type == Token::CloseAngle) {
                    parser->token_index++;
                    tmpl.close_angle = close_angle;
                }
            }
            tmpl.child_decl = Heap::create<Declaration>(parse_declaration_internal(parser, enclosing_class_name));
        } else if (token.text == "using") {
            // using directive or type alias
            parser->token_index++;
            parser->mute_errors = false;
            Token token2 = read_token(parser);
            if (token2.type == Token::Identifier && token2.text == "namespace") {
                auto& using_dir = result.var.switch_to<Declaration::UsingNamespace>();
                using_dir.using_keyword = token;
                using_dir.namespace_keyword = token2;
                using_dir.qid = parse_qualified_id(parser, ParseQualifiedMode::RequireComplete);
            } else {
                auto& alias = result.var.switch_to<Declaration::TypeAlias>();
                alias.using_keyword = token;
                alias.name = token2;

                Token equal_token = peek_token(parser);
                if (equal_token.type != Token::SingleEqual) {
                    // expected =
                    parser->error(Error, equal_token.input_offset,
                                  FMT_MSG("expected '=' before '{}'", equal_token.to_string()));
                } else {
                    parser->token_index++;
                    alias.equals = equal_token;
                    alias.type_id = parse_type_id(parser);
                }
            }
        } else {
            result = parse_entity_declaration(parser, enclosing_class_name);
        }
    } else if (token.type == Token::Semicolon) {
        parser->token_index++;
        /*
        Declaration::Empty empty;
        empty.semicolon = token;
        Declaration decl;
        decl.var = std::move(empty);
        add_declaration_to_current_scope(parser, std::move(decl));
        */
    } else if (token.type == Token::Tilde) {
        result = parse_entity_declaration(parser, enclosing_class_name);
    } else {
        parser->token_index++;
        parser->error(Error, token.input_offset, FMT_MSG("expected declaration before '{}'", token.to_string()));
    }
    return result;
}

Array<Declaration> parse_declaration_list(ParserImpl* parser, Token* out_close_curly, StringView enclosing_class_name) {
    // Always handle close curly at this scope, even if it's file scope:
    SetAcceptFlagsInScope accept_scope{parser, Token::OpenCurly};
    Array<Declaration> result;

    for (;;) {
        Token token = peek_token(parser);
        if (token.type == Token::EOF) {
            if (out_close_curly) {
                parser->error(Error, token.input_offset, FMT_MSG("expected '}' before '{}'", token.to_string()));
            }
            break;
        } else if (token.type == Token::CloseCurly) {
            parser->token_index++;
            if (out_close_curly) {
                *out_close_curly = token;
                break;
            }
            parser->error(Error, token.input_offset, FMT_MSG("expected declaration before '{}'", token.to_string()));
            continue;
        }

        result.append(parse_declaration_internal(parser, enclosing_class_name));

        bool semicolon_required = true;
        if (auto* entity = result.back().var.as<Declaration::Entity>()) {
            if (entity->init_declarators.num_items() > 0) {
                semicolon_required = !entity->init_declarators.back().init.var.is<Initializer::FunctionBody>();
            }
        }

        Token semicolon = peek_token(parser);
        if (semicolon.type == Token::Semicolon) {
            parser->token_index++;
            parser->mute_errors = false;
        } else if (semicolon_required) {
            parser->error(Error, semicolon.input_offset, FMT_MSG("expected ';' before '{}'", semicolon.to_string()));
        }
    }
    return result;
}

Array<Declaration> parse_translation_unit(ParserImpl* parser) {
    Array<Declaration> result = parse_declaration_list(parser, nullptr, {});
    Token eof_tok = peek_token(parser);
    PLY_ASSERT(eof_tok.type == Token::EOF); // EOF is the only possible token here
    PLY_UNUSED(eof_tok);
    return result;
}

//  ▄▄▄▄▄                       ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀
//  ▄▄▄▄▄                                           ▄▄
//  ██    ▄▄  ▄▄ ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄
//  ██▀▀   ▀██▀  ██  ██ ██  ▀▀ ██▄▄██ ▀█▄▄▄  ▀█▄▄▄  ██ ██  ██ ██  ██ ▀█▄▄▄
//  ██▄▄▄ ▄█▀▀█▄ ██▄▄█▀ ██     ▀█▄▄▄   ▄▄▄█▀  ▄▄▄█▀ ██ ▀█▄▄█▀ ██  ██  ▄▄▄█▀
//               ██

void consume_specifier(ParserImpl* parser) {
    for (;;) {
        Token token = peek_token(parser);
        if (token.type == Token::OpenAngle) {
            // Template type
            // FIXME: Does < always indicate a template type here?
            // FIXME: This needs to handle "Tmpl<(2 > 1)>" and ""Tmpl<(2 >> 1)>"
            parser->token_index++;
            PLY_SET_IN_SCOPE(parser->tkr.config.tokenize_right_shift, false);
            Token close_token;
            skip_any_scope(parser, &close_token, token);
            token = peek_token(parser);
        }
        if (token.type == Token::DoubleColon) {
            parser->token_index++;
            Token spec_token = peek_token(parser);
            if (spec_token.type == Token::Identifier) {
                parser->token_index++;
            } else {
                // expected identifier after ::
                parser->error(Error, spec_token.input_offset,
                              FMT_MSG("expected identifier before '{}'", spec_token.to_string()));
                return;
            }
        } else
            return;
    }
}

void parse_capture_list(ParserImpl* parser) {
    Token token = read_token(parser);
    if (token.type != Token::CloseSquare) {
        // FIXME: accept an actual capture list instead of just an empty list
        parser->error(Error, token.input_offset, FMT_MSG("expected ']' before '{}'", token.to_string()));
    }
}

// FIXME: This needs work.
// It's enough to parse the initializers used by Plywood, but there are definitely lots of
// expressions it doesn't handle.
ParsedExpression parse_expression(ParserImpl* parser, bool optional) {
    Token start_token = read_token(parser);
    Token end_token;
    switch (start_token.type) {
        case Token::Identifier: {
            // FIXME: This should use parse_qualified_id instead
            consume_specifier(parser);
            Token token2 = peek_token(parser);
            if (token2.type == Token::OpenParen) {
                // Function arguments
                parser->token_index++;
                SetAcceptFlagsInScope accept_scope{parser, Token::OpenParen};
                for (;;) {
                    Token token3 = peek_token(parser);
                    if (token3.type == Token::CloseParen) {
                        parser->token_index++;
                        end_token = token3;
                        break; // end of arguments
                    } else {
                        parse_expression(parser);
                        Token token4 = read_token(parser);
                        if (token4.type == Token::Comma) {
                        } else if (token4.type == Token::CloseParen) {
                            end_token = token4;
                            break; // end of arguments
                        } else {
                            // expected , or ) after argument
                            parser->error(Error, token4.input_offset,
                                          FMT_MSG("expected ',' or ')' before '{}'", token4.to_string()));
                            if (!handle_unexpected_token(parser, nullptr, token4))
                                break;
                        }
                    }
                }
            } else if (token2.type == Token::OpenCurly) {
                // It's a braced initializer (list).
                // FIXME: Not sure, but maybe this case should use a "low priority" curly (???)
                // Because if ';' is encountered, we should perhaps end the outer declaration.
                // And if an outer ) is matched, it should maybe cancel the initializer.
                // However, if we do that, it will be inconsisent with the behavior of
                // skip_any_scope(). Does that matter?
                parser->token_index++;
                SetAcceptFlagsInScope accept_scope{parser, Token::OpenCurly};
                for (;;) {
                    Token token3 = peek_token(parser);
                    if (token3.type == Token::CloseCurly) {
                        parser->token_index++;
                        end_token = token3;
                        break; // end of arguments
                    } else {
                        parse_expression(parser);
                        Token token4 = read_token(parser);
                        if (token4.type == Token::Comma) {
                        } else if (token4.type == Token::CloseCurly) {
                            end_token = token4;
                            break; // end of arguments
                        } else {
                            // expected , or } after argument
                            parser->error(Error, token4.input_offset,
                                          FMT_MSG("expected ',' or '}}' before '{}'", token4.to_string()));
                            if (!handle_unexpected_token(parser, nullptr, token4))
                                break;
                        }
                    }
                }
            } else {
                // Can't consume any more of expression
                end_token = start_token;
            }
            break;
        }

        case Token::NumericLiteral: {
            // Consume it
            end_token = start_token;
            break;
        }

        case Token::StringLiteral: {
            end_token = start_token;
            for (;;) {
                // concatenate multiple string literals
                Token token = peek_token(parser);
                if (token.type != Token::StringLiteral)
                    break;
                parser->token_index++;
                end_token = token;
            }
            break;
        }

        case Token::OpenParen: {
            SetAcceptFlagsInScope accept_scope{parser, Token::OpenParen};
            parse_expression(parser);
            Token token2 = peek_token(parser);
            if (token2.type == Token::CloseParen) {
                // Treat as a C-style cast.
                // FIXME: This should only be done if the inner expression identifies a type!
                // Otherwise, it's just a parenthesized expression:
                parser->token_index++;
                end_token = parse_expression(parser, true).end_token;
            } else {
                // expected ) after expression
                Token close_paren;
                close_scope(parser, &close_paren, start_token); // This will log an error
                end_token = close_paren;
            }
            break;
        }

        case Token::OpenCurly: {
            for (;;) {
                Token token2 = peek_token(parser);
                if (token2.type == Token::CloseCurly) {
                    parser->token_index++;
                    end_token = token2;
                    break;
                } else {
                    parse_expression(parser);
                    Token token4 = read_token(parser);
                    if (token4.type == Token::Comma) {
                    } else if (token4.type == Token::CloseCurly) {
                        end_token = token4;
                        break; // end of braced initializer
                    } else {
                        // expected , or } after expression
                        parser->error(Error, token4.input_offset,
                                      FMT_MSG("expected ',' or '}}' before '{}'", token4.to_string()));
                        if (!handle_unexpected_token(parser, nullptr, token4))
                            break;
                    }
                }
            }
            break;
        }

        case Token::Bang:
        case Token::SingleAmpersand:
        case Token::SingleMinus: {
            end_token = parse_expression(parser).end_token;
            break;
        }

        case Token::OpenSquare: {
            // lambda expression
            parse_capture_list(parser);
            Token open_paren = peek_token(parser);
            if (open_paren.type == Token::OpenParen) {
                parser->token_index++;
                Array<Parameter> unused_params;
                parse_parameter_declaration_list(parser, &unused_params, false);
                Token close_paren = peek_token(parser);
                if (close_paren.type == Token::CloseParen) {
                    parser->token_index++;
                }
            } else {
                parser->error(Error, open_paren.input_offset,
                              FMT_MSG("expected '(' before '{}'", open_paren.to_string()));
            }
            Token token2 = peek_token(parser);
            if (token2.type == Token::Arrow) {
                parser->token_index++;
                Declaration::Entity entity;
                parse_type_id(parser);
                token2 = peek_token(parser);
            }
            if (token2.type != Token::OpenCurly) {
                parser->error(Error, token2.input_offset, FMT_MSG("expected '{{' before '{}'", token2.to_string()));
            } else {
                parser->token_index++;
                Token close_token;
                skip_any_scope(parser, &close_token, token2);
                end_token = close_token;
            }
            break;
        }

        default: {
            if (optional) {
                parser->token_index--;
            } else {
                PLY_ASSERT(0);
            }
            return {};
        }
    }

    Token token = peek_token(parser);
    switch (token.type) {
        case Token::CloseAngle: {
            if (!parser->tkr.config.tokenize_right_shift) {
                break;
            } else {
                parser->token_index++;
                end_token = parse_expression(parser).end_token;
            }
        };

        case Token::SingleVerticalBar:
        case Token::DoubleEqual:
        case Token::NotEqual:
        case Token::OpenAngle:
        case Token::LessThanOrEqual:
        case Token::GreaterThanOrEqual:
        case Token::LeftShift:
        case Token::RightShift:
        case Token::SinglePlus:
        case Token::SingleMinus:
        case Token::Percent:
        case Token::Arrow:
        case Token::Star:
        case Token::Dot:
        case Token::ForwardSlash: {
            parser->token_index++;
            end_token = parse_expression(parser).end_token;
            break;
        }

        case Token::QuestionMark: {
            parser->token_index++;
            parse_expression(parser);
            token = peek_token(parser);
            if (token.type != Token::SingleColon) {
                // expected : after expression
                // FIXME: It would be cool the mention, in the error message, that the colon is
                // needed to match the '?' that was encountered earlier
                parser->error(Error, token.input_offset, FMT_MSG("expected ':' before '{}'", token.to_string()));
            } else {
                parser->token_index++;
                end_token = parse_expression(parser).end_token;
            }
            break;
        };

        default:
            break;
    }
    return {start_token, end_token};
}

//  ▄▄▄▄▄         ▄▄     ▄▄▄  ▄▄            ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄
//  ██  ██ ▄▄  ▄▄ ██▄▄▄   ██  ▄▄  ▄▄▄▄     ██  ██ ██  ██  ██
//  ██▀▀▀  ██  ██ ██  ██  ██  ██ ██        ██▀▀██ ██▀▀▀   ██
//  ██     ▀█▄▄██ ██▄▄█▀ ▄██▄ ██ ▀█▄▄▄     ██  ██ ██     ▄██▄
//

Owned<Parser> Parser::create() {
    return Heap::create<ParserImpl>();
}

void Parser::destroy() {
    Heap::destroy(static_cast<ParserImpl*>(this));
}

void set_input(ParserImpl* parser, StringView abs_path, StringView contents) {
    Preprocessor::File& file = parser->pp.files.append();
    file.abs_path = abs_path;
    file.contents = contents;
    file.token_loc_map = TokenLocationMap::create_from_string(contents);

    parser->pp.input_ranges.insert({});

    Preprocessor::IncludedItem& item = parser->pp.include_stack.append();
    item.vin = ViewStream{file.contents};
}

void apply_preprocessor_definitions(ParserImpl* parser) {
    for (PreprocessorDefinition& def : parser->predefined_defs) {
        // Add to macro_map.
        u32 macro_idx = parser->pp.macros.num_items();
        PLY_ASSERT(!parser->pp.macro_map.find(def.name)); // Adding twice is probably a mistake.
        *parser->pp.macro_map.insert(def.name).value = macro_idx;

        // Add to macros.
        Preprocessor::Macro& macro = parser->pp.macros.append();
        macro.name = def.name;
        macro.expansion = def.expansion;
    }
}

PreprocessResult Parser::preprocess(StringView abs_path, StringView src) {
    ParserImpl* parser = static_cast<ParserImpl*>(this);
    set_input(parser, abs_path, src);
    apply_preprocessor_definitions(parser);
    parser->is_only_preprocessing = true;

    MemStream mem;
    for (;;) {
        Token token = read_token(parser);
        if (token.type == Token::EOF) {
            break;
        }
        mem.write(token.to_string());
    }

    PreprocessResult result;
    result.output = mem.move_to_string();
    result.success = parser->success;
    result.diagnostics = std::move(parser->diagnostics);
    return result;
}

ParseResult Parser::parse_file(StringView abs_path, StringView src) {
    ParserImpl* parser = static_cast<ParserImpl*>(this);
    set_input(parser, abs_path, src);
    apply_preprocessor_definitions(parser);

    ParseResult result;
    result.declarations = parse_translation_unit(parser);
    result.success = parser->success;
    result.diagnostics = std::move(parser->diagnostics);
    return result;
}

Declaration Parser::parse_declaration(StringView input, StringView enclosing_class_name) {
    ParserImpl* parser = static_cast<ParserImpl*>(this);
    set_input(parser, {}, input);
    apply_preprocessor_definitions(parser);
    return parse_declaration_internal(parser, enclosing_class_name);
}

FileLocation Parser::get_file_location(u32 input_offset) const {
    const ParserImpl* parser = static_cast<const ParserImpl*>(this);
    return cpp::get_file_location(&parser->pp, input_offset);
}

//   ▄▄▄▄                 ▄▄
//  ██  ▀▀ ▄▄  ▄▄ ▄▄▄▄▄  ▄██▄▄  ▄▄▄▄  ▄▄  ▄▄
//   ▀▀▀█▄ ██  ██ ██  ██  ██    ▄▄▄██  ▀██▀
//  ▀█▄▄█▀ ▀█▄▄██ ██  ██  ▀█▄▄ ▀█▄▄██ ▄█▀▀█▄
//          ▄▄▄█▀
//  ▄▄  ▄▄ ▄▄        ▄▄     ▄▄▄  ▄▄        ▄▄      ▄▄   ▄▄
//  ██  ██ ▄▄  ▄▄▄▄▄ ██▄▄▄   ██  ▄▄  ▄▄▄▄▄ ██▄▄▄  ▄██▄▄ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀██ ██ ██  ██ ██  ██  ██  ██ ██  ██ ██  ██  ██   ██ ██  ██ ██  ██
//  ██  ██ ██ ▀█▄▄██ ██  ██ ▄██▄ ██ ▀█▄▄██ ██  ██  ▀█▄▄ ██ ██  ██ ▀█▄▄██
//             ▄▄▄█▀                 ▄▄▄█▀                         ▄▄▄█▀

struct NodeVisitor {
    const ParserImpl* parser = nullptr;
    Array<TokenSpan> spans;
    const QualifiedID* inside_qid = nullptr;
    bool needs_space = false;

    void append(TokenSpan::Color color, const Token& token) {
        TokenSpan& span = this->spans.append();
        span.color = color;
        span.token = token;
        span.qid = inside_qid;
    }
    void append_space() {
        TokenSpan& span = this->spans.append();
        span.is_space = true;
        span.qid = inside_qid;
    }
};

void syntax_highlight_decl_specifiers(NodeVisitor* visitor, ArrayView<const Owned<DeclSpecifier>> decl_specifiers);
void syntax_highlight_declarator(NodeVisitor* visitor, Variant<const QualifiedID*, const Token*> name, const DeclProduction* prod);

void syntax_highlight_qid(NodeVisitor* visitor, TokenSpan::Color color, const QualifiedID& qid) {
    PLY_SET_IN_SCOPE(visitor->inside_qid, &qid);
    for (const QualifiedID::Prefix& p : qid.prefix) {
        if (auto* ident = p.var.as<QualifiedID::Identifier>()) {
            visitor->append(TokenSpan::Type, ident->name);
        } else if (auto* tmpl_id = p.var.as<QualifiedID::TemplateID>()) {
            visitor->append(TokenSpan::Type, tmpl_id->name);
            visitor->append(TokenSpan::None, tmpl_id->open_angle);
            visitor->needs_space = false;
            for (const QualifiedID::TemplateID::Arg& arg : tmpl_id->args) {
                if (auto* type_id = arg.var.as<TypeID>()) {
                    syntax_highlight_decl_specifiers(visitor, type_id->decl_specifiers);
                    syntax_highlight_declarator(visitor, {}, type_id->abstract_dcor);
                }
            }
            visitor->append(TokenSpan::None, tmpl_id->close_angle);
        } else {
            PLY_ASSERT(0); // Not supported yet
        }
        if (p.double_colon.is_valid()) {
            visitor->append(TokenSpan::None, p.double_colon);
        }
    }

    if (auto* ident = qid.var.as<QualifiedID::Identifier>()) {
        visitor->append(color, ident->name);
    } else if (auto* tmpl_id = qid.var.as<QualifiedID::TemplateID>()) {
        visitor->append(color, tmpl_id->name);
        visitor->append(TokenSpan::None, tmpl_id->open_angle);
        visitor->needs_space = false;
        for (const QualifiedID::TemplateID::Arg& arg : tmpl_id->args) {
            if (auto* type_id = arg.var.as<TypeID>()) {
                syntax_highlight_decl_specifiers(visitor, type_id->decl_specifiers);
                syntax_highlight_declarator(visitor, {}, type_id->abstract_dcor);
            }
        }
        visitor->append(TokenSpan::None, tmpl_id->close_angle);
    } else if (auto* dtor = qid.var.as<QualifiedID::Destructor>()) {
        visitor->append(color, dtor->tilde);
        visitor->append(color, dtor->name);
    } else if (auto* op_func = qid.var.as<QualifiedID::OperatorFunc>()) {
        visitor->append(color, op_func->keyword);
        visitor->append(color, op_func->punc);
        if (op_func->punc2.is_valid()) {
            visitor->append(color, op_func->punc2);
        }
    } else if (auto* conv_func = qid.var.as<QualifiedID::ConversionFunc>()) {
        visitor->append(color, conv_func->operator_keyword);
        visitor->needs_space = true;
        syntax_highlight_decl_specifiers(visitor, conv_func->decl_specifiers);
        syntax_highlight_declarator(visitor, {}, conv_func->abstract_dcor);
    } else {
        PLY_ASSERT(0); // Not supported yet
    }
}

void syntax_highlight_decl_specifiers(NodeVisitor* visitor, ArrayView<const Owned<DeclSpecifier>> decl_specifiers) {
    for (const DeclSpecifier* decl_spec : decl_specifiers) {
        if (visitor->needs_space) {
            visitor->append_space();
        }
        if (auto* keyword = decl_spec->var.as<DeclSpecifier::Keyword>()) {
            visitor->append(TokenSpan::None, keyword->token);
        } else if (auto* type_id = decl_spec->var.as<DeclSpecifier::TypeSpecifier>()) {
            if (type_id->elaborate_keyword.is_valid()) {
                visitor->append(TokenSpan::None, type_id->elaborate_keyword);
            }
            syntax_highlight_qid(visitor, TokenSpan::Type, type_id->qid);
        } else if (auto* type_param = decl_spec->var.as<DeclSpecifier::TypeParameter>()) {
            visitor->append(TokenSpan::None, type_param->keyword);
            if (type_param->ellipsis.is_valid()) {
                visitor->append(TokenSpan::None, type_param->ellipsis);
            }
        }
        visitor->needs_space = true;
    }
}

void syntax_highlight_declarator(NodeVisitor* visitor, Variant<const QualifiedID*, const Token*> name, const DeclProduction* prod) {
    // First, flatten the chain.
    // FIXME: We should really do this at parse time.
    Array<const DeclProduction*> prod_chain;
    for (const DeclProduction* p = prod; p; p = p->child) {
        prod_chain.append(p);
    }

    // Next, create parentheses groups.
    struct ParenGroup {
        u32 first;
        u32 leading;
        u32 last;
    };
    Array<ParenGroup> paren_groups;
    {
        u32 first = 0;
        s32 trailing = -1;
        for (u32 i = 0; i < prod_chain.num_items(); i++) {
            if (prod_chain[i]->var.is<DeclProduction::ArrayOf>() || prod_chain[i]->var.is<DeclProduction::Function>()) {
                trailing = i;
            }
            if (prod_chain[i]->var.is<DeclProduction::Parenthesized>()) {
                return; // FIXME
                paren_groups.append({first, (u32) (trailing + 1), i});
                first = i + 1;
                trailing = first;
            }
        }
        paren_groups.append({first, (u32) (trailing + 1), prod_chain.num_items()});
    }

    // Visit leading productions of each group.
    for (s32 g = paren_groups.num_items() - 1; g >= 0; g--) {
        const ParenGroup& group = paren_groups[g];
        for (s32 i = group.last - 1; i >= (s32) group.leading; i--) {
            if (auto* indirect = prod_chain[i]->var.as<DeclProduction::Indirection>()) {
                visitor->append(TokenSpan::None, indirect->punc);
            } else if (auto* qualifier = prod_chain[i]->var.as<DeclProduction::Qualifier>()) {
                if (visitor->needs_space) {
                    visitor->append_space();
                }
                visitor->append(TokenSpan::None, qualifier->keyword);
                visitor->needs_space = true;
            } else {
                PLY_ASSERT(0);
            }
        }
        if (g > 0) {
            // Open parenthesis
            PLY_ASSERT(group.first > 0);
            auto* paren = prod_chain[group.first - 1]->var.as<DeclProduction::Parenthesized>();
            if (visitor->needs_space) {
                visitor->append_space();
            }
            visitor->append(TokenSpan::None, paren->open_paren);
            visitor->needs_space = false;
        }
    }

    // Visit qualified-id.
    if (const Token** token = name.as<const Token*>()) {
        if (visitor->needs_space) {
            visitor->append_space();
        }
        visitor->append(TokenSpan::Variable, **token);
        visitor->needs_space = true;
    } else if (const QualifiedID** qid = name.as<const QualifiedID*>()) {
        if (visitor->needs_space) {
            visitor->append_space();
        }
        syntax_highlight_qid(visitor, TokenSpan::Symbol, **qid);
        visitor->needs_space = true;
    }

    // Visit trailing productions of each group.
    for (u32 g = 0; g < paren_groups.num_items(); g++) {
        const ParenGroup& group = paren_groups[g];
        for (u32 i = group.first; i < group.leading; i++) {
            if (auto* array_of = prod_chain[i]->var.as<DeclProduction::ArrayOf>()) {
                visitor->append(TokenSpan::None, array_of->open_square);
                visitor->append(TokenSpan::None, array_of->close_square);
                visitor->needs_space = false;
            } else if (auto* function = prod_chain[i]->var.as<DeclProduction::Function>()) {
                visitor->append(TokenSpan::None, function->open_paren);
                visitor->needs_space = false;
                // Visit function parameters.
                for (const Parameter& param : function->params) {
                    syntax_highlight_decl_specifiers(visitor, param.decl_specifiers);
                    syntax_highlight_declarator(visitor, &param.identifier, param.prod);
                    if (param.comma.is_valid()) {
                        visitor->append(TokenSpan::None, param.comma);
                        visitor->append_space();
                    }
                }
                visitor->append(TokenSpan::None, function->close_paren);
                for (const Token& token : function->qualifiers) {
                    visitor->append_space();
                    visitor->append(TokenSpan::None, token);
                }
                visitor->needs_space = true;
            } else {
                PLY_ASSERT(0);
            }
        }
        if (g + 1 < paren_groups.num_items()) {
            // Close parenthesis
            auto* paren = prod_chain[group.last]->var.as<DeclProduction::Parenthesized>();
            visitor->append(TokenSpan::None, paren->close_paren);
            visitor->needs_space = true;
        }
    }
}

void syntax_highlight_initializer(NodeVisitor* visitor, const Initializer& init) {
    if (init.var.as<Initializer::Assignment>()) {
        // Not supported yet
    } else if (init.var.as<Initializer::FunctionBody>()) {
        // Not supported yet
    } else if (init.var.as<Initializer::BitField>()) {
        // Not supported yet
    }
}

void syntax_highlight_declaration(NodeVisitor* visitor, const Declaration& decl) {
    if (auto* entity = decl.var.as<Declaration::Entity>()) {
        syntax_highlight_decl_specifiers(visitor, entity->decl_specifiers);
        for (const InitDeclarator& init_decl : entity->init_declarators) {
            syntax_highlight_declarator(visitor, &init_decl.qid, init_decl.prod);
            syntax_highlight_initializer(visitor, init_decl.init);
            if (init_decl.comma.is_valid()) {
                visitor->append(TokenSpan::None, init_decl.comma);
                visitor->append_space();
            }
        }
    } else if (auto* tmpl = decl.var.as<Declaration::Template>()) {
        visitor->append(TokenSpan::None, tmpl->keyword);
        visitor->append_space();
        visitor->append(TokenSpan::None, tmpl->open_angle);
        visitor->needs_space = false;
        for (const Parameter& param : tmpl->params) {
            syntax_highlight_decl_specifiers(visitor, param.decl_specifiers);
            syntax_highlight_declarator(visitor, &param.identifier, param.prod);
            if (param.comma.is_valid()) {
                visitor->append(TokenSpan::None, param.comma);
                visitor->append_space();
            }
        }
        visitor->append(TokenSpan::None, tmpl->close_angle);
        visitor->needs_space = true;
        syntax_highlight_declaration(visitor, *tmpl->child_decl);
    }
}

Array<TokenSpan> Parser::syntax_highlight(const Declaration& decl) const {
    NodeVisitor visitor;
    visitor.parser = static_cast<const ParserImpl*>(this);
    syntax_highlight_declaration(&visitor, decl);
    return std::move(visitor.spans);
}

//  ▄▄▄▄▄         ▄▄                        ▄▄▄▄          ▄▄                  ▄▄
//  ██  ██  ▄▄▄▄  ██▄▄▄  ▄▄  ▄▄  ▄▄▄▄▄     ██  ██ ▄▄  ▄▄ ▄██▄▄ ▄▄▄▄▄  ▄▄  ▄▄ ▄██▄▄
//  ██  ██ ██▄▄██ ██  ██ ██  ██ ██  ██     ██  ██ ██  ██  ██   ██  ██ ██  ██  ██
//  ██▄▄█▀ ▀█▄▄▄  ██▄▄█▀ ▀█▄▄██ ▀█▄▄██     ▀█▄▄█▀ ▀█▄▄██  ▀█▄▄ ██▄▄█▀ ▀█▄▄██  ▀█▄▄
//                               ▄▄▄█▀                         ██

struct DumpContext {
    Stream* out = nullptr;
    const ParserImpl* parser = nullptr;
    u32 indent_level = 0;

    String indent() const {
        return StringView{"  "} * this->indent_level;
    }
};

void dump_declaration(DumpContext& ctx, const Declaration& decl);
void dump_expression(DumpContext& ctx, const Expression* expr);
void dump_statement(DumpContext& ctx, const Statement& stmt);

void dump_decl_specifier(DumpContext& ctx, const DeclSpecifier& decl_spec) {
    using Var = decltype(decl_spec.var);
    switch (decl_spec.var.get_subtype_index()) {
        case Var::index_of<DeclSpecifier::Keyword>: {
            auto* keyword = decl_spec.var.as<DeclSpecifier::Keyword>();
            ctx.out->format("{}Keyword '{}'\n", ctx.indent(), keyword->token.text);
            break;
        }
        case Var::index_of<DeclSpecifier::Linkage>: {
            auto* lang_linkage = decl_spec.var.as<DeclSpecifier::Linkage>();
            ctx.out->format("{}Linkage '{}'\n", ctx.indent(), lang_linkage->literal.text);
            break;
        }
        case Var::index_of<DeclSpecifier::Class>: {
            auto* class_ = decl_spec.var.as<DeclSpecifier::Class>();
            ctx.out->format("{}Class {} '{}'\n", ctx.indent(), class_->keyword.text, to_string(class_->qid));
            if (class_->virt_specifiers.num_items() > 0) {
                ctx.out->format("{}  virt_specifiers:", ctx.indent());
                for (const Token& virt_spec : class_->virt_specifiers) {
                    ctx.out->format(" {}", virt_spec.text);
                }
                ctx.out->write("\n");
            }
            if (class_->base_specifiers.num_items() > 0) {
                ctx.out->format("{}  base_specifiers:", ctx.indent());
                StringView comma;
                for (const DeclSpecifier::Class::BaseSpecifier& base_spec : class_->base_specifiers) {
                    ctx.out->format("{} {} {}", comma, base_spec.access_spec.text, to_string(base_spec.base_qid));
                    comma = ",";
                }
                ctx.out->write("\n");
            }
            PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
            for (const Declaration& decl : class_->member_decls) {
                dump_declaration(ctx, decl);
            }
            break;
        }
        case Var::index_of<DeclSpecifier::Enum>: {
            auto* enum_ = decl_spec.var.as<DeclSpecifier::Enum>();
            ctx.out->format("{}Enum{}{} '{}'\n", ctx.indent(), enum_->class_keyword.is_valid() ? " " : "",
                            enum_->class_keyword.text, to_string(enum_->qid));
            if (!enum_->base.is_empty()) {
                ctx.out->format("{}  base: '{}'\n", ctx.indent(), to_string(enum_->base));
            }
            for (const DeclSpecifier::Enum::Item& enor : enum_->enumerators) {
                ctx.out->format("{}  '{}'\n", ctx.indent(), enor.text.text);
                PLY_ASSERT(enor.init.var.is_empty()); // Not supported yet
            }
            break;
        }
        case Var::index_of<DeclSpecifier::TypeSpecifier>: {
            auto* type_spec = decl_spec.var.as<DeclSpecifier::TypeSpecifier>();
            ctx.out->format("{}TypeSpecifier '{}'\n", ctx.indent(), to_string(type_spec->qid));
            break;
        }
        default: {
            PLY_ASSERT(0); // Not supported yet
            break;
        }
    }
}

void dump_declarator_production(DumpContext& ctx, const DeclProduction* prod) {
    if (!prod)
        return;

    using Var = decltype(prod->var);
    switch (prod->var.get_subtype_index()) {
        case Var::index_of<DeclProduction::Parenthesized>: {
            ctx.out->format("{}Parenthesized\n", ctx.indent());
            break;
        }
        case Var::index_of<DeclProduction::Indirection>: {
            auto* pointer_to = prod->var.as<DeclProduction::Indirection>();
            ctx.out->format("{}Indirection ", ctx.indent());
            PLY_ASSERT(pointer_to->prefix.is_empty()); // Not supported yet
            ctx.out->format("'{}'\n", pointer_to->punc.text);
            break;
        }
        case Var::index_of<DeclProduction::ArrayOf>: {
            //auto* array_of = prod->var.as<DeclProduction::ArrayOf>();
            ctx.out->format("{}ArrayOf\n", ctx.indent());
            // FIXME: dump size
            break;
        }
        case Var::index_of<DeclProduction::Function>: {
            auto* function = prod->var.as<DeclProduction::Function>();
            ctx.out->format("{}Function\n", ctx.indent());
            if (!function->params.is_empty()) {
                PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
                for (const Parameter& param : function->params) {
                    ctx.out->format("{}Parameter '{}'\n", ctx.indent(), param.identifier.text);
                    PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
                    for (const DeclSpecifier* decl_spec : param.decl_specifiers) {
                        dump_decl_specifier(ctx, *decl_spec);
                    }
                    dump_declarator_production(ctx, param.prod);
                    PLY_ASSERT(param.init.var.is_empty()); // Not supported yet
                }
            }
            //          PLY_ASSERT(function->qualifiers.tokens.is_empty()); // Not supported yet
            // PLY_ASSERT(is_empty(function->trailing_ret_type)); // Not supported yet
            break;
        }
        case Var::index_of<DeclProduction::Qualifier>: {
            auto* qualifier = prod->var.as<DeclProduction::Qualifier>();
            ctx.out->format("{}Qualifier '{}'\n", ctx.indent(), qualifier->keyword.text);
            break;
        }
        default: {
            PLY_ASSERT(0); // Invalid
            break;
        }
    }
    PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
    dump_declarator_production(ctx, prod->child);
}

void dump_init_declarator(DumpContext& ctx, const InitDeclarator& init_decl) {
    ctx.out->format("{}InitDeclarator '{}'\n", ctx.indent(), to_string(init_decl.qid));
    {
        PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
        dump_declarator_production(ctx, init_decl.prod);
    }
    using Var = decltype(init_decl.init.var);
    switch (init_decl.init.var.get_subtype_index()) {
        case 0: {
            break; // Empty
        }
        case Var::index_of<Initializer::Assignment>: {
            auto* assignment = init_decl.init.var.as<Initializer::Assignment>();
            if (auto* expression = assignment->var.as<Owned<Expression>>()) {
                ctx.out->format("{}Assignment (expression)\n", ctx.indent());
                PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
                dump_expression(ctx, expression->get());
            } else if (auto* type_id = assignment->var.as<TypeID>()) {
                ctx.out->format("{}Assignment (type_id)\n", ctx.indent());
                PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
                for (const DeclSpecifier* decl_spec : type_id->decl_specifiers) {
                    dump_decl_specifier(ctx, *decl_spec);
                }
                dump_declarator_production(ctx, type_id->abstract_dcor);
            } else {
                PLY_ASSERT(0);
            }
            break;
        }
        case Var::index_of<Initializer::FunctionBody>: {
            auto* function_body = init_decl.init.var.as<Initializer::FunctionBody>();
            ctx.out->format("{}FunctionBody\n", ctx.indent());
            PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
            for (const Initializer::FunctionBody::MemberInitializer& member_init : function_body->member_inits) {
                ctx.out->format("{}MemberInitializer '{}'\n", ctx.indent(), to_string(member_init.qid));
                PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
                dump_expression(ctx, member_init.expr);
            }
            for (const Statement& statement : function_body->statements) {
                dump_statement(ctx, statement);
            }
            break;
        }
        case Var::index_of<Initializer::BitField>: {
            auto* bit_field = init_decl.init.var.as<Initializer::BitField>();
            ctx.out->format("{}BitField\n", ctx.indent());
            PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
            dump_expression(ctx, bit_field->expr);
            break;
        }
        default: {
            PLY_ASSERT(0); // Invalid
            break;
        }
    }
}

void dump_declaration(DumpContext& ctx, const Declaration& decl) {
    auto format_loc = [&](const Token& token) {
        FileLocation file_loc = ctx.parser->get_file_location(token.input_offset);
        return String::format("{}({})", split_path(file_loc.abs_path).filename, file_loc.line);
    };
    using Var = decltype(decl.var);
    switch (decl.var.get_subtype_index()) {
        case Var::index_of<Declaration::Linkage>: {
            auto* linkage = decl.var.as<Declaration::Linkage>();
            ctx.out->format("{}{}: Linkage '{}'\n", ctx.indent(), format_loc(linkage->extern_keyword),
                            linkage->literal.text);
            PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
            for (const Declaration& decl : linkage->child_decls) {
                dump_declaration(ctx, decl);
            }
            break;
        }
        case Var::index_of<Declaration::Namespace>: {
            auto* ns = decl.var.as<Declaration::Namespace>();
            ctx.out->format("{}{}: Namespace '{}'\n", ctx.indent(), format_loc(ns->keyword), to_string(ns->qid));
            PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
            for (const Declaration& decl : ns->child_decls) {
                dump_declaration(ctx, decl);
            }
            break;
        }
        case Var::index_of<Declaration::Entity>: {
            auto* entity = decl.var.as<Declaration::Entity>();
            ctx.out->format("{}{}: Entity\n", ctx.indent(), format_loc(get_first_token(*entity)));
            PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
            for (const DeclSpecifier* decl_spec : entity->decl_specifiers) {
                dump_decl_specifier(ctx, *decl_spec);
            }
            for (const InitDeclarator& init_decl : entity->init_declarators) {
                dump_init_declarator(ctx, init_decl);
            }
            break;
        }
        case Var::index_of<Declaration::Template>: {
            auto* tmpl = decl.var.as<Declaration::Template>();
            ctx.out->format("{}{}: Template'\n", ctx.indent(), format_loc(tmpl->keyword));
            PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
            dump_declaration(ctx, *tmpl->child_decl);
            break;
        }
        case Var::index_of<Declaration::TypeAlias>: {
            auto* alias = decl.var.as<Declaration::TypeAlias>();
            ctx.out->format("{}{}: TypeAlias '{}'\n", ctx.indent(), format_loc(alias->using_keyword), alias->name.text);
            PLY_SET_IN_SCOPE(ctx.indent_level, ctx.indent_level + 1);
            for (const DeclSpecifier* decl_spec : alias->type_id.decl_specifiers) {
                dump_decl_specifier(ctx, *decl_spec);
            }
            dump_declarator_production(ctx, alias->type_id.abstract_dcor);
            break;
        }
        case Var::index_of<Declaration::UsingNamespace>: {
            auto* using_directive = decl.var.as<Declaration::UsingNamespace>();
            ctx.out->format("{}{}: UsingNamespace '{}'\n", ctx.indent(), format_loc(using_directive->using_keyword),
                            to_string(using_directive->qid));
            break;
        }
        case Var::index_of<Declaration::StaticAssert>: {
            auto* static_assert_ = decl.var.as<Declaration::StaticAssert>();
            ctx.out->format("{}{}: StaticAssert\n", ctx.indent(), format_loc(static_assert_->keyword));
            // Dump expression
            break;
        }
        case Var::index_of<Declaration::AccessSpecifier>: {
            auto* access_spec = decl.var.as<Declaration::AccessSpecifier>();
            ctx.out->format("{}{}: AccessSpecifier '{}'\n", ctx.indent(), format_loc(access_spec->keyword),
                            access_spec->keyword.text);
            break;
        }
        case 0: {
            ctx.out->format("{}{}: Declaration (empty)\n", ctx.indent(), format_loc(decl.semicolon));
            break;
        }
        default: {
            PLY_ASSERT(0);
            break;
        }
    }
}

void dump_expression(DumpContext& ctx, const Expression* expr) {
}

void dump_statement(DumpContext& ctx, const Statement& stmt) {
}

void Parser::dump_declaration(const Declaration& decl) const {
    const ParserImpl* parser = static_cast<const ParserImpl*>(this);
    Stream out = get_stdout();
    DumpContext ctx;
    ctx.out = &out;
    ctx.parser = parser;
    cpp::dump_declaration(ctx, decl);
}

} // namespace cpp
} // namespace ply
