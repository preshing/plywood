/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    test-suite                                        │
│               Documentation: /docs/sample-apps/test-suite.md    │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

#include <ply-cpp.h>

using namespace ply;
using namespace ply::cpp;

// Generates the complete output for the C++ parser golden tests.
static String generateParserTestOutput(StringView testSuitePath) {
    Stream in = FileSystem::openTextForReadAutodetect(testSuitePath);
    MemStream out;
    for (;;) {
        String line;
        for (;;) {
            line = readLine(in);
            if (!line)
                break;
            if (line.startsWith(">>"))
                break;
        }
        if (!line)
            break;

        out.write(line);
        MemStream src;
        for (;;) {
            line = readLine(in);
            if (!line)
                break;
            if (line.startsWith("--"))
                break;
            src.write(line);
            out.write(line);
        }
        out.write("--\n");

        Owned<Parser> parser = Parser::create();
        ParseResult result = parser->parseFile({}, src.moveToString());
        if (result.diagnostics) {
            for (StringView diag : result.diagnostics) {
                out.write(diag);
            }
        }
        out.write("\n\n");
    }
    in.close();

    return out.moveToString();
}

// Generates the complete output for the C++ preprocessor golden tests.
static String generatePreprocessorTestOutput(StringView testSuitePath) {
    Stream in = FileSystem::openTextForReadAutodetect(testSuitePath);
    MemStream out;
    for (;;) {
        String line;
        for (;;) {
            line = readLine(in);
            if (!line)
                break;
            if (line.startsWith(">>"))
                break;
        }
        if (!line)
            break;

        out.write(line);
        MemStream src;
        for (;;) {
            line = readLine(in);
            if (!line)
                break;
            if (line.startsWith("--"))
                break;
            src.write(line);
            out.write(line);
        }
        out.write("--\n");

        Owned<Parser> parser = Parser::create();
        PreprocessResult result = parser->preprocess("<test file>", src.moveToString());
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

    return out.moveToString();
}

// Compares or rewrites one C++ golden test file.
static bool processCppTestFile(StringView path, bool write, String (*generateOutput)(StringView)) {
    String generated = generateOutput(path);
    Stream out = getStdOut();
    if (write) {
        FileSystem::saveText(path, generated);
        out.format("Rewrote {}\n", path);
        return true;
    }

    String expected = FileSystem::loadTextAutodetect(path);
    if (generated != expected) {
        out.format("***FAIL*** {} differs from generated output\n", path);
        return false;
    }
    out.format("success: {}\n", path);
    return true;
}

// Runs both C++ golden test suites, optionally rewriting their expected output.
bool runCppTests(bool write) {
    String parserPath = joinPath(TEST_SUITE_PATH, "cpp-parser-tests.txt");
    String preprocessorPath = joinPath(TEST_SUITE_PATH, "cpp-preprocessor-tests.txt");

    bool success = processCppTestFile(parserPath, write, generateParserTestOutput);
    if (!processCppTestFile(preprocessorPath, write, generatePreprocessorTestOutput))
        success = false;
    return success;
}
