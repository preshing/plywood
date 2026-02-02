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
#if defined(PLY_WINDOWS)
    SetConsoleOutputCP(CP_UTF8);
#endif

    String path = joinPath(MARKDOWN_TESTS_PATH, "spec.json");
    String src = Filesystem::loadTextAutodetect(path);
    json::Parser::Result result = json::Parser{}.parse(path, src);

    for (const json::Node* testCase : result.root->array_) {
        String converted = markdown::convertToHtml(testCase->get("markdown")->text());
        getStdout().write("---------------------\n");
        getStdout().write(converted);
        getStdout().write(testCase->get("html")->text());
    }
}
