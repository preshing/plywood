/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include <ply-tokenizer.h>

using namespace ply;

enum ConventionUsed {
    SnakeCase = 0,
    CamelCase,
    PascalCase,
    SpacedPascalCase,
    OtherCase
};

inline bool is_upper(char c) {
    return c >= 'A' && c <= 'Z';
}
inline bool is_lower(char c) {
    return c >= 'a' && c <= 'z';
}

ConventionUsed get_convention(StringView str) {
    bool possible[4] = { false };
    if (str.num_bytes() < 1)
        return OtherCase;
    if (is_lower(str[0])) {
        possible[SnakeCase] = true;
        possible[CamelCase] = true;
    } else if (is_upper(str[0])) {
        possible[PascalCase] = true;
        possible[SpacedPascalCase] = true;
    }
    for (const char* s = str.begin(); s < str.end() - 1; s++) {
        if (is_decimal_digit(s[1])) {
            possible[SnakeCase] = false;
            possible[CamelCase] = false;
        } else if (is_upper(s[1])) {
            possible[SnakeCase] = false;
            if (is_lower(s[0])) {
                possible[SpacedPascalCase] = false;
            } else if (is_upper(s[0])) {
                return OtherCase;
            }
        } else if (s[1] == '_') {
            possible[CamelCase] = false;
            possible[PascalCase] = false;
        }
    }
    for (u32 i = 0; i < PLY_STATIC_ARRAY_SIZE(possible); i++) {
        if (possible[i])
            return (ConventionUsed) i;
    }
    return OtherCase;
}

Set<String> all_tokens;

Array<StringView> ignore = {
    "cFileName",
    "Coordinated_Universal_Time",
    "Eastern_Time_Zone",
    "Unix_time",
};

void process_file(StringView full_path) {
    TextFormat format;
    String contents = Filesystem::load_text_autodetect(full_path, &format);
    if (!contents)
        return;

    bool any_changes = false;
    MemStream mem;

    Tokenizer tkr;
    tkr.config.tokenize_preprocessor_directives = false;
    tkr.config.tokenize_c_style_comments = false;
    tkr.config.tokenize_line_comments = false;
    tkr.config.tokenize_single_quoted_strings = false;
    tkr.config.tokenize_double_quoted_strings = false;
    ViewStream input{contents};
    while (Token token = read_token(tkr, input)) {
        if (token.type == Token::Identifier) {
            ConventionUsed conv = get_convention(token.text);
            if (conv == OtherCase) {
//                all_tokens.insert(token.text);
            }

            if (conv == SpacedPascalCase) {
                /*
                if (!token.text.ends_with("_t") && !token.text.ends_with("_") && !token.text.ends_with("_T") && (find(ignore, token.text) < 0)) {
                    all_tokens.insert(token.text);
                    for (char c : token.text) {
                        if (c != '_') {
                            mem.write(c);
                        }
                    }
                    any_changes = true;
                    continue;
                }
                */
            }
            if (conv == CamelCase) {
                if (!token.text.starts_with("ai") && !token.text.starts_with("gl") && !token.text.starts_with("lp") && !token.text.starts_with("dw") && !token.text.starts_with("ft") && (find(ignore, token.text) < 0)) {
                    if (!(token.text.num_bytes() > 1 && is_upper(token.text[1]))) {
                        all_tokens.insert(token.text);
                        for (char c : token.text) {
                            if (is_upper(c)) {
                                mem.write('_');
                                mem.write(StringView{c}.lower());
                            } else {
                                mem.write(c);
                            }
                        }
                        any_changes = true;
                        continue;
                    }
                }
            }
        }
        mem.write(token.text);
    }

    if (any_changes) {
        Filesystem::save_text(full_path, mem.move_to_string(), format);
    }
}

int main(int argc, const char* argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    for (WalkTriple& triple : Filesystem::walk(join_path(PLYWOOD_ROOT_DIR, ".."))) {
        for (u32 i = 0; i < triple.dir_names.num_items();) {
            if (triple.dir_names[i].starts_with("build") || triple.dir_names[i].starts_with(".") || (triple.dir_names[i] == "soloud") || (triple.dir_names[i] == "extern")) {
                triple.dir_names.erase(i);
            } else {
                i++;
            }
        }
        for (const DirectoryEntry& entry : triple.files) {
            if (!entry.name.starts_with("ply-base.cpp"))
                continue;
            if (find<const StringView>({".h", ".cpp", ".inl", ".md", ".natvis"}, split_file_extension(entry.name).extension) >= 0) {
                String full_path = join_path(triple.dir_path, entry.name);
                get_stdout().format("{}\n", full_path);
                process_file(full_path);
            } else {
            }
        }
    }

    Array<StringView> sorted;
    for (StringView str : all_tokens) {
        sorted.append(str);
    }
    sort(sorted);

    Stream out = get_stdout();
    for (StringView str : sorted) {
        out.format("{}\n", str);
    }

    return 0;
}
