/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    test-suite                                        │
│               Documentation: docs/apps/test-suite.md            │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

#pragma once

#include <ply-reflect.h>

using namespace ply;

// Stores the command-line options shared by every compiled-in test suite.
struct CommandLineOptions {
    bool runSystem = false;
    bool runNetwork = false;
    bool runUnicode = false;
    bool runMarkdown = false;
    bool runCpp = false;
    bool regenCpp = false;
    bool runTranscript = false;
    bool runFragmentation = false;
    bool runAll = false;
    bool verbose = false;
    bool printUsage = false;
    PLY_DECLARE_TYPE_INFO(CommandLineOptions)
};

// Exposes the options populated once by the executable entry point.
extern CommandLineOptions options;

// Stores the number of successful and executed tests in one category.
struct TestResult {
    u32 numPassed = 0;
    u32 numRun = 0;

    void add(bool success) {
        this->numPassed += success ? 1 : 0;
        this->numRun++;
    }

    void add(const TestResult& other) {
        this->numPassed += other.numPassed;
        this->numRun += other.numRun;
    }

    bool isSuccess() const {
        return this->numPassed == this->numRun;
    }
};

#if WITH_SYSTEM_TESTS
TestResult runSystemTests();
#endif
#if WITH_NETWORK_TESTS
TestResult runNetworkTests();
#endif
#if WITH_UNICODE_LOADING_TESTS
TestResult runUnicodeLoadingTests();
#endif
#if WITH_MARKDOWN_TESTS
TestResult runMarkdownTests();
#endif
#if WITH_CPP_TESTS
TestResult runCppTests(bool write);
#endif
#if WITH_TRANSCRIPT_TESTS
TestResult runTranscriptTests();
#endif
#if WITH_FRAGMENTATION_TEST
TestResult runFragmentationTest();
#endif
