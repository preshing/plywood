/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    test-suite                                        │
│               Documentation: /docs/apps/test-suite.md           │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

#include <ply-reflect.h>
#include "test-suite.h"

#if WITH_SYSTEM_TESTS || WITH_NETWORK_TESTS || WITH_UNICODE_LOADING_TESTS
#include "run-system-tests.h"
#endif

using namespace ply;

// Provides every test-suite translation unit with the parsed command-line options.
CommandLineOptions options;

// Identifies a test suite and distinguishes normal C++ tests from golden-file generation.
enum class TestSuite {
    System,
    Network,
    Unicode,
    Markdown,
    Cpp,
    RegenCpp,
    Transcript,
    Fragmentation,
};

// Prints usage containing only the options registered for this executable.
static void printUsage(Stream& out, StringView executablePath, const CommandLineParser& parser) {
    out.format("Usage: {} <options>\n", executablePath);
    out.write("Options may be combined and run in command-line order:\n");
    parser.printAvailableOptions(out);
}

// Returns the logical suite index used to reject duplicate selections.
static u32 getLogicalSuiteIndex(TestSuite suite) {
    switch (suite) {
        case TestSuite::System:
            return 0;
        case TestSuite::Network:
            return 1;
        case TestSuite::Unicode:
            return 2;
        case TestSuite::Markdown:
            return 3;
        case TestSuite::Cpp:
        case TestSuite::RegenCpp:
            return 4;
        case TestSuite::Transcript:
            return 5;
        case TestSuite::Fragmentation:
            return 6;
    }
    PLY_ASSERT(0);
    return 0;
}

// Runs one selected suite and returns its counted result.
static TestResult runTestSuite(TestSuite suite) {
    switch (suite) {
#if WITH_SYSTEM_TESTS
        case TestSuite::System:
            if (options.verbose) {
                getStdOut().write("\nSystem tests\n");
            }
            return runSystemTests();
#endif
#if WITH_NETWORK_TESTS
        case TestSuite::Network:
            if (options.verbose) {
                getStdOut().write("\nNetwork tests\n");
            }
            return runNetworkTests();
#endif
#if WITH_UNICODE_LOADING_TESTS
        case TestSuite::Unicode:
            if (options.verbose) {
                getStdOut().write("\nUnicode loading tests\n");
            }
            return runUnicodeLoadingTests();
#endif
#if WITH_MARKDOWN_TESTS
        case TestSuite::Markdown:
            if (options.verbose) {
                getStdOut().write("\nMarkdown tests\n");
            }
            return runMarkdownTests();
#endif
#if WITH_CPP_TESTS
        case TestSuite::Cpp:
            if (options.verbose) {
                getStdOut().write("\nC++ tests\n");
            }
            return runCppTests(false);
        case TestSuite::RegenCpp:
            if (options.verbose) {
                getStdOut().write("\nWriting C++ golden files\n");
            }
            return runCppTests(true);
#endif
#if WITH_TRANSCRIPT_TESTS
        case TestSuite::Transcript:
            if (options.verbose) {
                getStdOut().write("\nTranscript tests\n");
            }
            return runTranscriptTests();
#endif
#if WITH_FRAGMENTATION_TEST
        case TestSuite::Fragmentation:
            if (options.verbose) {
                getStdOut().write("\nFragmentation test\n");
            }
            return runFragmentationTest();
#endif
        default:
            PLY_ASSERT(0);
            return {};
    }
}

// Prints the compact result line for one completed category.
static void printTestSuiteResult(TestSuite suite, const TestResult& result) {
    StringView name;
    switch (suite) {
        case TestSuite::System:
            name = "System tests";
            break;
        case TestSuite::Network:
            name = "Network tests";
            break;
        case TestSuite::Unicode:
            name = "Unicode loading tests";
            break;
        case TestSuite::Markdown:
            name = "Markdown tests";
            break;
        case TestSuite::Cpp:
            name = "C++ tests";
            break;
        case TestSuite::RegenCpp:
            name = "C++ golden files";
            break;
        case TestSuite::Transcript:
            name = "Transcript tests";
            break;
        case TestSuite::Fragmentation:
            name = "Fragmentation test";
            break;
    }
    StringView action = suite == TestSuite::RegenCpp ? "rewritten" : "passed";
    getStdOut().format("{}: {}/{} {}{}\n", name, result.numPassed, result.numRun, action,
                       result.isSuccess() ? "" : " ***FAIL***");
}

// Parses the requested suites, preserves their order and aggregates their results.
int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    // Configure terminal output for UTF-8 test data.
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Let CommandLineParser validate the complete set of compiled-in options.
    CommandLineParser parser({
#if WITH_SYSTEM_TESTS
        {"-s", "--system", PLY_LOOKUP_MEMBER(CommandLineOptions, runSystem), "Run the system test suite"},
#endif
#if WITH_NETWORK_TESTS
        {"-n", "--network", PLY_LOOKUP_MEMBER(CommandLineOptions, runNetwork), "Run the network test suite"},
#endif
#if WITH_UNICODE_LOADING_TESTS
        {"-u", "--unicode", PLY_LOOKUP_MEMBER(CommandLineOptions, runUnicode),
         "Run the Unicode file loading test suite"},
#endif
#if WITH_MARKDOWN_TESTS
        {"-m", "--markdown", PLY_LOOKUP_MEMBER(CommandLineOptions, runMarkdown), "Run the Markdown test suite"},
#endif
#if WITH_CPP_TESTS
        {"-c", "--cpp", PLY_LOOKUP_MEMBER(CommandLineOptions, runCpp),
         "Run the C++ parser and preprocessor test suites"},
        {"-r", "--regen-cpp", PLY_LOOKUP_MEMBER(CommandLineOptions, regenCpp),
         "Regenerate the C++ parser and preprocessor golden files"},
#endif
#if WITH_TRANSCRIPT_TESTS
        {"-t", "--transcript", PLY_LOOKUP_MEMBER(CommandLineOptions, runTranscript), "Run the transcript tests"},
#endif
#if WITH_FRAGMENTATION_TEST
        {"-f", "--fragmentation", PLY_LOOKUP_MEMBER(CommandLineOptions, runFragmentation),
         "Run the fragmentation test suite"},
#endif
        {"-a", "--all", PLY_LOOKUP_MEMBER(CommandLineOptions, runAll),
         "Run every compiled-in suite; may be combined with --verbose"},
        {"-v", "--verbose", PLY_LOOKUP_MEMBER(CommandLineOptions, verbose), "Print detailed test progress and output"},
        {"-h", "--help", PLY_LOOKUP_MEMBER(CommandLineOptions, printUsage), "Print this help"},
    });
    if (!parser.apply(argc, argv, &options)) {
        Stream err = getStdErr();
        printUsage(err, argv[0], parser);
        return 1;
    }
    if (options.printUsage) {
        Stream out = getStdOut();
        printUsage(out, argv[0], parser);
        return 0;
    }

    // Expand --all in canonical order, or rebuild the requested order from argv.
    Array<TestSuite> suites;
    if (options.runAll) {
        if (argc != (options.verbose ? 3 : 2)) {
            getStdErr().write("Option --all may only be combined with --verbose.\n");
            Stream err = getStdErr();
            printUsage(err, argv[0], parser);
            return 1;
        }
#if WITH_SYSTEM_TESTS
        suites.append(TestSuite::System);
#endif
#if WITH_NETWORK_TESTS
        suites.append(TestSuite::Network);
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
        bool selectedSuites[7] = {};
        for (int i = 1; i < argc; i++) {
            StringView arg = argv[i];
            if (arg == "-v" || arg == "--verbose") {
                continue;
            }
            TestSuite suite;
            if (arg == "-s" || arg == "--system") {
                suite = TestSuite::System;
            } else if (arg == "-n" || arg == "--network") {
                suite = TestSuite::Network;
            } else if (arg == "-u" || arg == "--unicode") {
                suite = TestSuite::Unicode;
            } else if (arg == "-m" || arg == "--markdown") {
                suite = TestSuite::Markdown;
            } else if (arg == "-c" || arg == "--cpp") {
                suite = TestSuite::Cpp;
            } else if (arg == "-r" || arg == "--regen-cpp") {
                suite = TestSuite::RegenCpp;
            } else if (arg == "-t" || arg == "--transcript") {
                suite = TestSuite::Transcript;
            } else {
                PLY_ASSERT(arg == "-f" || arg == "--fragmentation");
                suite = TestSuite::Fragmentation;
            }

            // --cpp and --regen-cpp select the same logical suite and cannot be combined.
            u32 logicalIndex = getLogicalSuiteIndex(suite);
            if (selectedSuites[logicalIndex]) {
                getStdErr().format("Test suite selected more than once: {}\n", arg);
                Stream err = getStdErr();
                printUsage(err, argv[0], parser);
                return 1;
            }
            selectedSuites[logicalIndex] = true;
            suites.append(suite);
        }
    }

    // An empty command line is an error rather than an implicit -all.
    if (!suites) {
        Stream err = getStdErr();
        printUsage(err, argv[0], parser);
        return 1;
    }

    // Continue through every suite while retaining any earlier failure.
    bool success = true;
    for (TestSuite suite : suites) {
        TestResult result = runTestSuite(suite);
        printTestSuiteResult(suite, result);
        if (!result.isSuccess()) {
            success = false;
        }
    }
    return success ? 0 : 1;
}

PLY_STRUCT_BEGIN(CommandLineOptions)
PLY_STRUCT_MEMBER(runSystem)
PLY_STRUCT_MEMBER(runNetwork)
PLY_STRUCT_MEMBER(runUnicode)
PLY_STRUCT_MEMBER(runMarkdown)
PLY_STRUCT_MEMBER(runCpp)
PLY_STRUCT_MEMBER(regenCpp)
PLY_STRUCT_MEMBER(runTranscript)
PLY_STRUCT_MEMBER(runFragmentation)
PLY_STRUCT_MEMBER(runAll)
PLY_STRUCT_MEMBER(verbose)
PLY_STRUCT_MEMBER(printUsage)
PLY_STRUCT_END()
