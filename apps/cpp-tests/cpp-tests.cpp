/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include <ply-cpp.h>

using namespace ply;
using namespace ply::cpp;

void run_parser_tests() {
    String test_suite_path = join_path(CPP_TESTS_PATH, "parser-tests.txt");
    Stream in = Filesystem::open_text_for_read_autodetect(test_suite_path);
    MemStream out;
    for (;;) {
        String line;
        while (line = read_line(in)) {
            if (line.starts_with(">>"))
                break;
        }
        if (!line)
            break;

        out.write(line);
        MemStream src;
        while (line = read_line(in)) {
            if (line.starts_with("--"))
                break;
            src.write(line);
            out.write(line);
        }
        out.write("--\n");

        Owned<Parser> parser = Parser::create();
        ParseResult result = parser->parse_file({}, src.move_to_string());
        if (result.diagnostics) {
            for (StringView diag : result.diagnostics) {
                out.write(diag);
            }
        }
        out.write("\n\n");
    }
    in.close();

    Filesystem::save_text(test_suite_path, out.move_to_string());
}

void run_preprocessor_tests() {
    String test_suite_path = join_path(CPP_TESTS_PATH, "preprocessor-tests.txt");
    Stream in = Filesystem::open_text_for_read_autodetect(test_suite_path);
    MemStream out;
    for (;;) {
        String line;
        while (line = read_line(in)) {
            if (line.starts_with(">>"))
                break;
        }
        if (!line)
            break;

        out.write(line);
        MemStream src;
        while (line = read_line(in)) {
            if (line.starts_with("--"))
                break;
            src.write(line);
            out.write(line);
        }
        out.write("--\n");

        Owned<Parser> parser = Parser::create();
        PreprocessResult result = parser->preprocess("<test file>", src.move_to_string());
        if (result.diagnostics) {
            for (StringView diag : result.diagnostics) {
                out.write(diag);
            }
        } else {
            out.write(result.output);
        }
        out.write("\n\n");
    }
    in.close();

    Filesystem::save_text(test_suite_path, out.move_to_string());
}

void parse_plywood_source() {
    String src_folder = join_path(CPP_TESTS_PATH, "../../src");
    String file_path = join_path(src_folder, "ply-base.h");
    String src = Filesystem::load_text_autodetect(file_path);
    Owned<Parser> parser = Parser::create();
    parser->include_paths.append(src_folder);
    ParseResult result = parser->parse_file(file_path, src);
    Stream out = get_stdout();
    for (StringView diagnostic : result.diagnostics) {
        out.write(diagnostic);
    }
    out.close();
    for (const Declaration& decl : result.declarations) {
        parser->dump_declaration(decl);
    }
}

void parse_this_file() {
    String src = Filesystem::load_text_autodetect(__FILE__);
    Owned<Parser> parser = Parser::create();
    ParseResult result = parser->parse_file(__FILE__, src);
    for (const Declaration& decl : result.declarations) {
        parser->dump_declaration(decl);
    }
}

int main(int argc, const char* argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    //run_parser_tests();
    run_preprocessor_tests();
    //parse_plywood_source();
    //parse_this_file();

    return 0;
}
