/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    test-suite                                        │
│               Documentation: /docs/apps/test-suite.md           │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

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

// Verifies that every CommonMark recognizer can be disabled without changing the default parsing mode.
static bool runMarkdownRecognitionOptionTests() {
    // Describes one source form and its output with the corresponding recognizer enabled and disabled.
    struct OptionTest {
        StringView name;
        bool markdown::ParseOptions::*flag;
        StringView source;
        StringView enabledHtml;
        StringView disabledHtml;
    };

    OptionTest tests[] = {
        {"backslashEscapes", &markdown::ParseOptions::backslashEscapes, "\\#\n", "<p>#</p>\n",
         "<p>\\#</p>\n"},
        {"characterReferences", &markdown::ParseOptions::characterReferences, "&#42;x&#42;\n", "<p>*x*</p>\n",
         "<p>&amp;#42;x&amp;#42;</p>\n"},
        {"codeSpans", &markdown::ParseOptions::codeSpans, "`x`\n", "<p><code>x</code></p>\n",
         "<p>`x`</p>\n"},
        {"emphasis", &markdown::ParseOptions::emphasis, "*x*\n", "<p><em>x</em></p>\n", "<p>*x*</p>\n"},
        {"strongEmphasis", &markdown::ParseOptions::strongEmphasis, "**x**\n", "<p><strong>x</strong></p>\n",
         "<p>**x**</p>\n"},
        {"inlineLinks", &markdown::ParseOptions::inlineLinks, "[x](/url)\n", "<p><a href=\"/url\">x</a></p>\n",
         "<p>[x](/url)</p>\n"},
        {"referenceLinks", &markdown::ParseOptions::referenceLinks, "[x]: /url\n\n[x]\n",
         "<p><a href=\"/url\">x</a></p>\n", "<p>[x]</p>\n"},
        {"inlineImages", &markdown::ParseOptions::inlineImages, "![x](/image)\n",
         "<p><img src=\"/image\" alt=\"x\" /></p>\n", "<p>![x](/image)</p>\n"},
        {"referenceImages", &markdown::ParseOptions::referenceImages, "[x]: /image\n\n![x]\n",
         "<p><img src=\"/image\" alt=\"x\" /></p>\n", "<p>![x]</p>\n"},
        {"autolinks", &markdown::ParseOptions::autolinks, "<https://example.com>\n",
         "<p><a href=\"https://example.com\">https://example.com</a></p>\n",
         "<p>&lt;https://example.com&gt;</p>\n"},
        {"inlineHTML", &markdown::ParseOptions::inlineHTML, "a <i>x</i>\n", "<p>a <i>x</i></p>\n",
         "<p>a &lt;i&gt;x&lt;/i&gt;</p>\n"},
        {"softLineBreaks", &markdown::ParseOptions::softLineBreaks, "a\nb\n", "<p>a\nb</p>\n",
         "<p>a\nb</p>\n"},
        {"hardLineBreaks", &markdown::ParseOptions::hardLineBreaks, "a  \nb\n", "<p>a<br />\nb</p>\n",
         "<p>a  \nb</p>\n"},
        {"blockQuotes", &markdown::ParseOptions::blockQuotes, "> quote\n",
         "<blockquote>\n<p>quote</p>\n</blockquote>\n", "<p>&gt; quote</p>\n"},
        {"orderedLists", &markdown::ParseOptions::orderedLists, "1. one\n", "<ol>\n<li>one</li>\n</ol>\n",
         "<p>1. one</p>\n"},
        {"unorderedLists", &markdown::ParseOptions::unorderedLists, "- one\n", "<ul>\n<li>one</li>\n</ul>\n",
         "<p>- one</p>\n"},
        {"indentedCodeBlocks", &markdown::ParseOptions::indentedCodeBlocks, "    code\n",
         "<pre><code>code\n</code></pre>\n", "<p>code</p>\n"},
        {"fencedCodeBlocks", &markdown::ParseOptions::fencedCodeBlocks, "~~~\ncode\n~~~\n",
         "<pre><code>code\n</code></pre>\n", "<p>~~~\ncode\n~~~</p>\n"},
        {"htmlBlocks", &markdown::ParseOptions::htmlBlocks, "<div>\nraw\n</div>\n", "<div>\nraw\n</div>\n",
         "<p><div>\nraw\n</div></p>\n"},
        {"atxHeadings", &markdown::ParseOptions::atxHeadings, "# title\n", "<h1>title</h1>\n",
         "<p># title</p>\n"},
        {"setextHeadings", &markdown::ParseOptions::setextHeadings, "title\n===\n", "<h1>title</h1>\n",
         "<p>title\n===</p>\n"},
        {"thematicBreaks", &markdown::ParseOptions::thematicBreaks, "***\n", "<hr />\n", "<p>***</p>\n"},
        {"linkReferenceDefinitions", &markdown::ParseOptions::linkReferenceDefinitions,
         "[a]: /url\n\n[a]\n", "<p><a href=\"/url\">a</a></p>\n", "<p>[a]: /url</p>\n<p>[a]</p>\n"},
    };

    // Verify default recognition and the result of disabling each option independently.
    bool allPassed = true;
    for (const OptionTest& test : tests) {
        String enabled = markdown::convertToHtml(test.source);
        markdown::ParseOptions options;
        options.*test.flag = false;
        String disabled = markdown::convertToHtml(test.source, options);
        if (enabled != test.enabledHtml || disabled != test.disabledHtml) {
            getStdOut().format("Markdown recognition option failure: {}\nExpected enabled:\n{}Actual enabled:\n{}"
                               "Expected disabled:\n{}Actual disabled:\n{}",
                               test.name, test.enabledHtml, enabled, test.disabledHtml, disabled);
            allPassed = false;
        }
    }

    // Soft breaks are observable in the span tree even though text newlines render identically in HTML.
    Array<Owned<markdown::Span>> softBreakSpans = markdown::parseInlineElements("a\nb");
    markdown::ParseOptions noSoftBreaks;
    noSoftBreaks.softLineBreaks = false;
    Array<Owned<markdown::Span>> plainNewlineSpans = markdown::parseInlineElements("a\nb", noSoftBreaks);
    bool softBreakPassed = softBreakSpans.numItems() == 3 &&
                           softBreakSpans[1]->var.is<markdown::Span::SoftBreak>() &&
                           plainNewlineSpans.numItems() == 1 && plainNewlineSpans[0]->var.is<markdown::Span::Text>() &&
                           plainNewlineSpans[0]->var.as<markdown::Span::Text>()->text == "a\nb";
    if (!softBreakPassed) {
        getStdOut().write("Markdown recognition option failure: softLineBreaks\n");
        allPassed = false;
    }

    getStdOut().format("Markdown recognition option checks {}\n", allPassed ? "passed" : "failed");
    return allPassed;
}

// Verifies the all-disabled preset and the block-free inline parsing entry point.
static bool runMarkdownInlineParsingTests() {
    // Check rendered output with every optional construct disabled.
    bool allPassed = true;
    markdown::ParseOptions none = markdown::ParseOptions::none();
    String noneHtml = markdown::convertToHtml("# *x* &amp;\n", none);
    if (noneHtml != "<p># *x* &amp;amp;</p>\n") {
        getStdOut().format("ParseOptions::none failure\nExpected:\n<p># *x* &amp;amp;</p>\nActual:\n{}", noneHtml);
        allPassed = false;
    }

    // Check that the same preset produces only fallback block and span types.
    Array<Owned<markdown::Block>> noneBlocks = markdown::parseWholeDocument("# *x* &amp;\n", none);
    bool noneTreePassed = noneBlocks.numItems() == 1 && noneBlocks[0]->var.is<markdown::Block::Paragraph>() &&
                          noneBlocks[0]->asLeaf()->spans.numItems() == 1 &&
                          noneBlocks[0]->asLeaf()->spans[0]->var.is<markdown::Span::Text>();
    if (!noneTreePassed) {
        getStdOut().write("ParseOptions::none tree failure\n");
        allPassed = false;
    }

    // Parse block-looking input directly as inline content while retaining enabled span recognition.
    Array<Owned<markdown::Span>> spans = markdown::parseInlineElements("# item\n- list\n`code`");
    bool inlinePassed = spans.numItems() == 5 && spans[0]->var.is<markdown::Span::Text>() &&
                        spans[1]->var.is<markdown::Span::SoftBreak>() &&
                        spans[2]->var.is<markdown::Span::Text>() &&
                        spans[3]->var.is<markdown::Span::SoftBreak>() && spans[4]->var.is<markdown::Span::Code>();
    if (!inlinePassed) {
        getStdOut().write("parseInlineElements block-isolation failure\n");
        allPassed = false;
    }

    // Convert inline source directly to HTML without introducing paragraph or other block markup.
    String inlineHtml = markdown::convertInlineToHtml("# item\n- list\n`code`");
    if (inlineHtml != "# item\n- list\n<code>code</code>") {
        getStdOut().format("convertInlineToHtml failure\nActual:\n{}\n", inlineHtml);
        allPassed = false;
    }

    // An explicitly disabled inline form must not fall back to an enabled shortcut-reference form.
    markdown::ParseOptions noInlineLinks;
    noInlineLinks.inlineLinks = false;
    String disabledInlineLink = markdown::convertToHtml("[x](/inline)\n\n[x]: /reference\n", noInlineLinks);
    if (disabledInlineLink != "<p>[x](/inline)</p>\n") {
        getStdOut().format("Disabled inline-link fallback failure\nActual:\n{}", disabledInlineLink);
        allPassed = false;
    }

    getStdOut().format("Markdown inline parsing checks {}\n", allPassed ? "passed" : "failed");
    return allPassed;
}

// Runs the CommonMark and GitHub Flavored Markdown conversion fixtures.
bool runMarkdownTests() {
    // Stop at the first failing child suite while preserving its failure result.
    bool success = true;
    success = success && runMarkdownTestFile("markdown-tests.txt", "CommonMark", {});
    success = success && runMarkdownTestFile("gfm-tests.txt", "GFM", markdown::ParseOptions::githubFlavored());
    success = success && runMarkdownOptionIsolationTests();
    success = success && runMarkdownRecognitionOptionTests();
    success = success && runMarkdownInlineParsingTests();
    return success;
}
