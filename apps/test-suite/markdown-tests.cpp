/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-markdown.h>

using namespace ply;

// Runs the Markdown conversion tests and returns true if every case passes.
bool runMarkdownTests() {
    // Open the markdown test suite.
    String path = joinPath(TEST_SUITE_PATH, "markdown-tests.txt");
    Stream in = Filesystem::openTextForReadAutodetect(path);
    String separatorLine = readLine(in);
    u32 numTests = 0;
    u32 numPassed = 0;
    while (separatorLine) {
        PLY_ASSERT(separatorLine.startsWith("--------------------- #"));
        String line;

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

        // Read expected HTML output until the next test-case marker or EOF.
        Array<String> expectedLines;
        for (;;) {
            line = readLine(in);
            if (!line)
                break;
            if (line.startsWith("--------------------- #"))
                break;
            expectedLines.append(line);
        }

        // Drop the blank line before the separator.
        if (expectedLines && expectedLines.back().trim().isEmpty()) {
            expectedLines.pop();
        }
        MemStream expectedHtml;
        for (StringView expectedLine : expectedLines) {
            expectedHtml.write(expectedLine);
        }

        // Convert markdown, update counters from an exact string match, and print case output.
        String converted = markdown::convertToHtml(markdownSrc.moveToString());
        String expected = expectedHtml.moveToString();
        numTests++;
        if (converted == expected) {
            numPassed++;
        }
        getStdOut().write(separatorLine);
        getStdOut().write(converted);
        getStdOut().format("({}/{} passed)\n", numPassed, numTests);

        // If we stopped on a separator line, carry that state into the next loop iteration.
        separatorLine = line;
    }

    in.close();
    getStdOut().format("{}/{} markdown tests passed\n", numPassed, numTests);
    return numPassed == numTests;
}
