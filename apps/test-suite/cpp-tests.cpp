/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    test-suite                                        │
│               Documentation: docs/apps/test-suite.md            │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

#include <ply-cpp.h>
#include "test-suite.h"

using namespace ply;
using namespace ply::cpp;

// Holds the fixture text belonging to one C++ golden test.
struct CppTestCase {
    String heading;
    String source;
    String expected;
};

// Reads one test case and retains only the next heading as lookahead.
static bool readCppTestCase(Stream& in, String& nextHeading, CppTestCase& testCase) {
    testCase = {};

    // Find the heading that starts the next case.
    String line = std::move(nextHeading);
    while (!line.startsWith(">>")) {
        line = readLine(in);
        if (!line)
            return false;
    }
    testCase.heading = std::move(line);

    // Read source lines through the expected-output separator.
    MemStream source;
    for (;;) {
        line = readLine(in);
        if (!line || line.startsWith("--"))
            break;
        source.write(line);
    }
    testCase.source = source.moveToString();

    // Read expected output while retaining the following case heading.
    MemStream expected;
    for (;;) {
        line = readLine(in);
        if (!line || line.startsWith(">>"))
            break;
        expected.write(line);
    }
    testCase.expected = expected.moveToString();
    nextHeading = std::move(line);
    return true;
}

// Runs or regenerates the C++ parser golden tests in one pass.
static TestResult runParserTests(StringView path, bool write) {
    Stream in = FileSystem::openTextForReadAutodetect(path);
    Stream out = getStdOut();
    MemStream rewritten;
    TestResult result;
    String nextHeading;
    CppTestCase testCase;

    // Generate and immediately process each parser case.
    while (readCppTestCase(in, nextHeading, testCase)) {
        // Collect the diagnostics produced for this source.
        MemStream generated;
        Owned<Parser> parser = Parser::create();
        ParseResult parseResult = parser->parseFile({}, testCase.source);
        if (parseResult.diagnostics) {
            for (StringView diag : parseResult.diagnostics) {
                generated.write(diag);
            }
        }
        generated.write("\n\n");
        String generatedOutput = generated.moveToString();

        // Rewrite the case or compare it with its expected output.
        if (write) {
            rewritten.write(testCase.heading);
            rewritten.write(testCase.source);
            rewritten.write("--\n");
            rewritten.write(generatedOutput);
            result.add(true);
        } else {
            bool success = generatedOutput == testCase.expected;
            result.add(success);
            if (!success) {
                out.format("***FAIL*** {}: {} differs from generated output\n", path,
                           testCase.heading.substr(2).trim());
            }
        }
    }
    in.close();

    // Save regenerated content only after the input fixture is closed.
    if (write) {
        FileSystem::saveText(path, rewritten.moveToString());
        if (options.verbose) {
            out.format("Rewrote {} ({} cases)\n", path, result.numRun);
        }
    } else if (options.verbose) {
        out.format("{}/{} cases passed: {}\n", result.numPassed, result.numRun, path);
    }
    return result;
}

// Runs or regenerates the C++ preprocessor golden tests in one pass.
static TestResult runPreprocessorTests(StringView path, bool write) {
    Stream in = FileSystem::openTextForReadAutodetect(path);
    Stream out = getStdOut();
    MemStream rewritten;
    TestResult result;
    String nextHeading;
    CppTestCase testCase;

    // Generate and immediately process each preprocessor case.
    while (readCppTestCase(in, nextHeading, testCase)) {
        // Collect the preprocessed output or diagnostics for this source.
        MemStream generated;
        Owned<Parser> parser = Parser::create();
        PreprocessResult preprocessResult = parser->preprocess("<test file>", testCase.source);
        if (preprocessResult.diagnostics) {
            for (StringView diag : preprocessResult.diagnostics) {
                generated.write(diag);
            }
        } else {
            generated.write(preprocessResult.output);
        }
        generated.write("\n\n");
        String generatedOutput = generated.moveToString();

        // Rewrite the case or compare it with its expected output.
        if (write) {
            rewritten.write(testCase.heading);
            rewritten.write(testCase.source);
            rewritten.write("--\n");
            rewritten.write(generatedOutput);
            result.add(true);
        } else {
            bool success = generatedOutput == testCase.expected;
            result.add(success);
            if (!success) {
                out.format("***FAIL*** {}: {} differs from generated output\n", path,
                           testCase.heading.substr(2).trim());
            }
        }
    }
    in.close();

    // Save regenerated content only after the input fixture is closed.
    if (write) {
        FileSystem::saveText(path, rewritten.moveToString());
        if (options.verbose) {
            out.format("Rewrote {} ({} cases)\n", path, result.numRun);
        }
    } else if (options.verbose) {
        out.format("{}/{} cases passed: {}\n", result.numPassed, result.numRun, path);
    }
    return result;
}

// Runs both C++ golden test suites, optionally rewriting their expected output.
TestResult runCppTests(bool write) {
    String parserPath = joinPath(TEST_SUITE_PATH, "cpp-parser-tests.txt");
    String preprocessorPath = joinPath(TEST_SUITE_PATH, "cpp-preprocessor-tests.txt");

    // Combine the results from both explicit runners.
    TestResult result;
    result.add(runParserTests(parserPath, write));
    result.add(runPreprocessorTests(preprocessorPath, write));
    return result;
}
