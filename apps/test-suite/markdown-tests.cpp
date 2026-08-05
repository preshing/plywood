/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-markdown.h>

using namespace ply;

// Runs one Markdown fixture file with the selected parsing options.
static bool runMarkdownTestFile(StringView fileName, StringView suiteName, const markdown::ParseOptions& options) {
    String path = joinPath(TEST_SUITE_PATH, fileName);
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
        String converted = markdown::convertToHtml(markdownSrc.moveToString(), options);
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
    getStdOut().format("{}/{} {} tests passed\n", numPassed, numTests, suiteName);
    return numPassed == numTests;
}

// Runs the CommonMark and GitHub Flavored Markdown conversion fixtures.
bool runMarkdownTests() {
    bool commonMarkPassed = runMarkdownTestFile("markdown-tests.txt", "CommonMark", {});
    bool gfmPassed = runMarkdownTestFile("gfm-tests.txt", "GFM", markdown::ParseOptions::githubFlavored());
    return commonMarkPassed && gfmPassed;
}
