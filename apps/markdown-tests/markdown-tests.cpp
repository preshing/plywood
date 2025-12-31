/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include <ply-json.h>
#include <ply-markdown.h>

using namespace ply;

int main(int argc, const char* argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    String path = join_path(MARKDOWN_TESTS_PATH, "spec.json");
    String src = Filesystem::load_text_autodetect(path);
    json::Parser::Result result = json::Parser{}.parse(path, src);

    for (const json::Node* test_case : result.root->array_) {
        String converted = markdown::convert_to_html(test_case->get("markdown")->text());
        get_stdout().write("---------------------\n");
        get_stdout().write(converted);
        get_stdout().write(test_case->get("html")->text());
    }
}
