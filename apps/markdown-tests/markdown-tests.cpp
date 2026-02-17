/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-markdown.h>

using namespace ply;

int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Open the serialized markdown test suite (">>" markdown, "--" html).
    String path = joinPath(MARKDOWN_TESTS_PATH, "test-suite.txt");
    Stream in = Filesystem::openTextForReadAutodetect(path);
    bool haveSeparator = false;
    u32 numTests = 0;
    u32 numPassed = 0;
    for (;;) {
        String line;

        // Find the next test case start marker (">>"), unless we've already consumed it.
        if (!haveSeparator) {
            for (;;) {
                line = readLine(in);
                if (!line)
                    break;
                if (line.startsWith(">> "))
                    break;
            }
            if (!line)
                break;
        } else {
            haveSeparator = false;
        }

        // Read markdown input until the expected-html marker ("--").
        MemStream markdownSrc;
        for (;;) {
            line = readLine(in);
            if (!line)
                break;
            if (line == "--\n")
                break;
            markdownSrc.write(line);
        }
        if (!line)
            break;

        // Read expected HTML output until the next test-case marker (">> ") or EOF.
        MemStream expectedHtml;
        for (;;) {
            line = readLine(in);
            if (!line)
                break;
            if (line.startsWith(">> "))
                break;
            expectedHtml.write(line);
        }

        // Convert markdown, update counters from an exact string match, and print case output.
        String converted = markdown::convertToHtml(markdownSrc.moveToString());
        String expected = expectedHtml.moveToString();
        numTests++;
        if (converted == expected)
            numPassed++;
        getStdOut().write("---------------------\n");
        getStdOut().write(converted);
        getStdOut().write(expected);
        getStdOut().format("({}/{} passed)\n", numPassed, numTests);

        // If we stopped on ">> ", carry that state into the next loop iteration.
        if (line)
            haveSeparator = true;
        else
            break;
    }

    in.close();
}
