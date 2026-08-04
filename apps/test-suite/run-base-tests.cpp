/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include "run-base-tests.h"

struct Case {
    StringView name;
    void (*func)();
    RegisterTest::Group group;
};

Array<Case>& getTestCases() {
    static Array<Case> cases;
    return cases;
}

RegisterTest::RegisterTest(StringView name, void (*func)(), Group group) {
    getTestCases().append({name, func, group});
}

struct TestState {
    bool success = true;
};

TestState gTestState;

bool check(bool cond) {
    if (!cond) {
        gTestState.success = false;
    }
    return cond;
}

// Runs the selected group of registered base tests.
static bool runTestGroup(RegisterTest::Group group) {
    u32 numPassed = 0;
    const auto& testCases = getTestCases();
    Stream out = getStdOut();
    u32 numTests = 0;

    // Count the selected tests so progress output uses a group-local total.
    for (const Case& testCase : testCases) {
        if (testCase.group == group)
            numTests++;
    }

    // Run the selected tests and check each one for leaked heap allocations.
    u32 testIndex = 0;
    for (const Case& testCase : testCases) {
        if (testCase.group != group)
            continue;
        out.format("[{}/{}] {}... ", ++testIndex, numTests, testCase.name);
        gTestState.success = true;

        // Check for memory leaks while running the test case.
        Heap::Stats beginStats = Heap::getStats();
        testCase.func();
        Heap::Stats endStats = Heap::getStats();
        if (beginStats.totalBytesConsumed != endStats.totalBytesConsumed) {
            gTestState.success = false;
        }

        out.write(gTestState.success ? "success\n" : "***FAIL***\n");
        if (gTestState.success) {
            numPassed++;
        }
        out.flush();
    }
    float frac = 1.f;
    if (numTests > 0)
        frac = (float) numPassed / numTests;
    out.format("{}/{} test cases passed ({}%)\n", numPassed, numTests, frac * 100.f);

    return numPassed == numTests;
}

// Runs the base test suite, excluding the file-loading tests.
bool runBaseTests() {
    return runTestGroup(RegisterTest::Base);
}

// Runs the Unicode text-file loading test suite.
bool runUnicodeLoadingTests() {
    return runTestGroup(RegisterTest::UnicodeLoading);
}
