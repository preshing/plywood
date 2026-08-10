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
    Stream in = FileSystem::openTextForReadAutodetect(path);
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

// Verifies that each GFM extension is disabled by default and can be enabled without enabling its peers.
static bool runMarkdownOptionIsolationTests() {
    // Describes one representative extension input and its output in both parsing modes.
    struct OptionTest {
        StringView name;
        bool markdown::ParseOptions::*flag;
        StringView source;
        StringView defaultHtml;
        StringView enabledHtml;
    };
    OptionTest tests[] = {
        {"tables", &markdown::ParseOptions::tables, "| a | b |\n| --- | --- |\n| c | d |\n",
         "<p>| a | b |\n| --- | --- |\n| c | d |</p>\n",
         "<table>\n<thead>\n<tr>\n<th>a</th>\n<th>b</th>\n</tr>\n</thead>\n<tbody>\n<tr>\n<td>c</td>\n"
         "<td>d</td>\n</tr>\n</tbody>\n</table>\n"},
        {"taskListItems", &markdown::ParseOptions::taskListItems, "- [x] done\n",
         "<ul>\n<li>[x] done</li>\n</ul>\n",
         "<ul>\n<li><input checked=\"\" disabled=\"\" type=\"checkbox\"> done</li>\n</ul>\n"},
        {"strikethrough", &markdown::ParseOptions::strikethrough, "~~gone~~\n", "<p>~~gone~~</p>\n",
         "<p><del>gone</del></p>\n"},
        {"extendedAutolinks", &markdown::ParseOptions::extendedAutolinks, "www.example.com\n",
         "<p>www.example.com</p>\n", "<p><a href=\"http://www.example.com\">www.example.com</a></p>\n"},
        {"tagFilter", &markdown::ParseOptions::tagFilter, "keep <title>unsafe</title>\n",
         "<p>keep <title>unsafe</title></p>\n", "<p>keep &lt;title>unsafe&lt;/title></p>\n"},
    };
    bool allPassed = true;

    // Check default output, then enable exactly one option and ensure every other extension stays disabled.
    for (u32 enabledIndex = 0; enabledIndex < PLY_STATIC_ARRAY_SIZE(tests); enabledIndex++) {
        const OptionTest& enabledTest = tests[enabledIndex];
        String defaultHtml = markdown::convertToHtml(enabledTest.source);
        if (defaultHtml != enabledTest.defaultHtml) {
            getStdOut().format("Markdown option isolation failure: {} is enabled by default\nExpected:\n{}Actual:\n{}",
                               enabledTest.name, enabledTest.defaultHtml, defaultHtml);
            allPassed = false;
        }

        markdown::ParseOptions options;
        options.*enabledTest.flag = true;
        for (u32 syntaxIndex = 0; syntaxIndex < PLY_STATIC_ARRAY_SIZE(tests); syntaxIndex++) {
            const OptionTest& syntaxTest = tests[syntaxIndex];
            StringView expected = syntaxIndex == enabledIndex ? syntaxTest.enabledHtml : syntaxTest.defaultHtml;
            String actual = markdown::convertToHtml(syntaxTest.source, options);
            if (actual != expected) {
                getStdOut().format("Markdown option isolation failure: enabling {} produced unexpected {} output\n"
                                   "Expected:\n{}Actual:\n{}",
                                   enabledTest.name, syntaxTest.name, expected, actual);
                allPassed = false;
            }
        }
    }
    getStdOut().format("Markdown option isolation checks {}\n", allPassed ? "passed" : "failed");
    return allPassed;
}

// Runs the CommonMark and GitHub Flavored Markdown conversion fixtures.
bool runMarkdownTests() {
    bool commonMarkPassed = runMarkdownTestFile("markdown-tests.txt", "CommonMark", {});
    bool gfmPassed = runMarkdownTestFile("gfm-tests.txt", "GFM", markdown::ParseOptions::githubFlavored());
    bool optionIsolationPassed = runMarkdownOptionIsolationTests();
    return commonMarkPassed && gfmPassed && optionIsolationPassed;
}
