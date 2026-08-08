/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-reflect.h>

#if WITH_SYSTEM_TESTS || WITH_UNICODE_LOADING_TESTS
#include "run-system-tests.h"
#endif

using namespace ply;

#if WITH_MARKDOWN_TESTS
bool runMarkdownTests();
#endif
#if WITH_CPP_TESTS
bool runCppTests(bool write);
#endif
#if WITH_TRANSCRIPT_TESTS
bool runTranscriptTests();
#endif
#if WITH_FRAGMENTATION_TEST
bool runFragmentationTest();
#endif

// Stores the options recognized by CommandLineParser.
struct CommandLineOptions {
    bool runSystem = false;
    bool runUnicode = false;
    bool runMarkdown = false;
    bool runCpp = false;
    bool regenCpp = false;
    bool runTranscript = false;
    bool runFragmentation = false;
    bool runAll = false;
    PLY_DECLARE_TYPE_INFO(CommandLineOptions)
};

// Identifies a test suite and distinguishes normal C++ tests from golden-file generation.
enum class TestSuite {
    System,
    Unicode,
    Markdown,
    Cpp,
    RegenCpp,
    Transcript,
    Fragmentation,
};

// Prints usage containing only the suites compiled into this executable.
static void printUsage(StringView executablePath) {
    Stream err = getStdErr();
    err.format("Usage: {} <options>\n", executablePath);
    err.write("Options may be combined and run in command-line order:\n");
#if WITH_SYSTEM_TESTS
    err.write("  -system     Run the system test suite\n");
#endif
#if WITH_UNICODE_LOADING_TESTS
    err.write("  -unicode    Run the Unicode file loading test suite\n");
#endif
#if WITH_MARKDOWN_TESTS
    err.write("  -markdown   Run the Markdown test suite\n");
#endif
#if WITH_CPP_TESTS
    err.write("  -cpp        Run the C++ parser and preprocessor test suites\n");
    err.write("  -regencpp   Regenerate the C++ parser and preprocessor golden files\n");
#endif
#if WITH_TRANSCRIPT_TESTS
    err.write("  -transcript Run the transcript tests\n");
#endif
#if WITH_FRAGMENTATION_TEST
    err.write("  -frag       Run the fragmentation test suite\n");
#endif
    err.write("  -all        Run every compiled-in suite; must be specified alone\n");
}

// Returns the logical suite index used to reject duplicate selections.
static u32 getLogicalSuiteIndex(TestSuite suite) {
    switch (suite) {
        case TestSuite::System:
            return 0;
        case TestSuite::Unicode:
            return 1;
        case TestSuite::Markdown:
            return 2;
        case TestSuite::Cpp:
        case TestSuite::RegenCpp:
            return 3;
        case TestSuite::Transcript:
            return 4;
        case TestSuite::Fragmentation:
            return 5;
    }
    PLY_ASSERT(0);
    return 0;
}

// Runs one selected suite and returns its pass/fail result.
static bool runTestSuite(TestSuite suite) {
    switch (suite) {
#if WITH_SYSTEM_TESTS
        case TestSuite::System:
            getStdOut().write("\nSystem tests\n");
            return runSystemTests();
#endif
#if WITH_UNICODE_LOADING_TESTS
        case TestSuite::Unicode:
            getStdOut().write("\nUnicode loading tests\n");
            return runUnicodeLoadingTests();
#endif
#if WITH_MARKDOWN_TESTS
        case TestSuite::Markdown:
            getStdOut().write("\nMarkdown tests\n");
            return runMarkdownTests();
#endif
#if WITH_CPP_TESTS
        case TestSuite::Cpp:
            getStdOut().write("\nC++ tests\n");
            return runCppTests(false);
        case TestSuite::RegenCpp:
            getStdOut().write("\nWriting C++ golden files\n");
            return runCppTests(true);
#endif
#if WITH_TRANSCRIPT_TESTS
        case TestSuite::Transcript:
            getStdOut().write("\nTranscript tests\n");
            return runTranscriptTests();
#endif
#if WITH_FRAGMENTATION_TEST
        case TestSuite::Fragmentation:
            getStdOut().write("\nFragmentation test\n");
            return runFragmentationTest();
#endif
        default:
            PLY_ASSERT(0);
            return false;
    }
}

// Parses the requested suites, preserves their order and aggregates their results.
int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    // Configure terminal output for UTF-8 test data.
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Let CommandLineParser validate the complete set of compiled-in options.
    CommandLineOptions options;
    CommandLineParser parser({
#if WITH_SYSTEM_TESTS
        {"-system", PLY_LOOKUP_MEMBER(CommandLineOptions, runSystem), "Run the system tests"},
#endif
#if WITH_UNICODE_LOADING_TESTS
        {"-unicode", PLY_LOOKUP_MEMBER(CommandLineOptions, runUnicode), "Run Unicode file loading tests"},
#endif
#if WITH_MARKDOWN_TESTS
        {"-markdown", PLY_LOOKUP_MEMBER(CommandLineOptions, runMarkdown), "Run Markdown tests"},
#endif
#if WITH_CPP_TESTS
        {"-cpp", PLY_LOOKUP_MEMBER(CommandLineOptions, runCpp), "Run C++ tests"},
        {"-regencpp", PLY_LOOKUP_MEMBER(CommandLineOptions, regenCpp), "Regenerate C++ golden files"},
#endif
#if WITH_TRANSCRIPT_TESTS
        {"-transcript", PLY_LOOKUP_MEMBER(CommandLineOptions, runTranscript), "Run transcript tests"},
#endif
#if WITH_FRAGMENTATION_TEST
        {"-frag", PLY_LOOKUP_MEMBER(CommandLineOptions, runFragmentation), "Run fragmentation tests"},
#endif
        {"-all", PLY_LOOKUP_MEMBER(CommandLineOptions, runAll), "Run all tests"},
    });
    if (!parser.apply(argc, argv, &options)) {
        printUsage(argv[0]);
        return 1;
    }

    // Expand -all in canonical order, or rebuild the requested order from argv.
    Array<TestSuite> suites;
    if (options.runAll) {
        if (argc != 2) {
            getStdErr().write("Option -all must be specified alone.\n");
            printUsage(argv[0]);
            return 1;
        }
#if WITH_SYSTEM_TESTS
        suites.append(TestSuite::System);
#endif
#if WITH_UNICODE_LOADING_TESTS
        suites.append(TestSuite::Unicode);
#endif
#if WITH_MARKDOWN_TESTS
        suites.append(TestSuite::Markdown);
#endif
#if WITH_CPP_TESTS
        suites.append(TestSuite::Cpp);
#endif
#if WITH_TRANSCRIPT_TESTS
        suites.append(TestSuite::Transcript);
#endif
#if WITH_FRAGMENTATION_TEST
        suites.append(TestSuite::Fragmentation);
#endif
    } else {
        bool selectedSuites[6] = {};
        for (int i = 1; i < argc; i++) {
            StringView arg = argv[i];
            TestSuite suite;
            if (arg == "-system") {
                suite = TestSuite::System;
            } else if (arg == "-unicode") {
                suite = TestSuite::Unicode;
            } else if (arg == "-markdown") {
                suite = TestSuite::Markdown;
            } else if (arg == "-cpp") {
                suite = TestSuite::Cpp;
            } else if (arg == "-regencpp") {
                suite = TestSuite::RegenCpp;
            } else if (arg == "-transcript") {
                suite = TestSuite::Transcript;
            } else {
                PLY_ASSERT(arg == "-frag");
                suite = TestSuite::Fragmentation;
            }

            // -cpp and -regencpp select the same logical suite and cannot be combined.
            u32 logicalIndex = getLogicalSuiteIndex(suite);
            if (selectedSuites[logicalIndex]) {
                getStdErr().format("Test suite selected more than once: {}\n", arg);
                printUsage(argv[0]);
                return 1;
            }
            selectedSuites[logicalIndex] = true;
            suites.append(suite);
        }
    }

    // An empty command line is an error rather than an implicit -all.
    if (!suites) {
        printUsage(argv[0]);
        return 1;
    }

    // Continue through every suite while retaining any earlier failure.
    bool success = true;
    for (TestSuite suite : suites) {
        if (!runTestSuite(suite))
            success = false;
    }
    return success ? 0 : 1;
}

PLY_STRUCT_BEGIN(CommandLineOptions)
PLY_STRUCT_MEMBER(runSystem)
PLY_STRUCT_MEMBER(runUnicode)
PLY_STRUCT_MEMBER(runMarkdown)
PLY_STRUCT_MEMBER(runCpp)
PLY_STRUCT_MEMBER(regenCpp)
PLY_STRUCT_MEMBER(runTranscript)
PLY_STRUCT_MEMBER(runFragmentation)
PLY_STRUCT_MEMBER(runAll)
PLY_STRUCT_END()
