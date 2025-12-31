/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include <ply-json.h>

using namespace ply;

int main(int argc, const char* argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    String path = join_path(JSON_TESTS_PATH, "test.json");
    String src = Filesystem::load_text_autodetect(path);

    json::Parser parser;
    auto result = parser.parse(path, src);

    return 0;
}
