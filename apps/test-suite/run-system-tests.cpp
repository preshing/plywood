/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    test-suite                                        │
│               Documentation: docs/apps/test-suite.md            │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

#include "run-system-tests.h"

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

// Runs the selected group of registered tests.
static TestResult runTestGroup(RegisterTest::Group group) {
    TestResult result;
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
        testIndex++;
        if (options.verbose) {
            out.format("[{}/{}] {}... ", testIndex, numTests, testCase.name);
        }
        gTestState.success = true;

        // Check for memory leaks while running the test case.
        Heap::Stats beginStats = Heap::getStats();
        testCase.func();
        Heap::Stats endStats = Heap::getStats();
        if (beginStats.totalBytesConsumed != endStats.totalBytesConsumed) {
            gTestState.success = false;
        }

        if (options.verbose) {
            out.write(gTestState.success ? "success\n" : "***FAIL***\n");
        } else if (!gTestState.success) {
            out.format("***FAIL*** [{}/{}] {}\n", testIndex, numTests, testCase.name);
        }
        result.add(gTestState.success);
        out.flush();
    }
    return result;
}

// Runs the system test suite, excluding the file-loading tests.
TestResult runSystemTests() {
    return runTestGroup(RegisterTest::System);
}

// Runs the network test suite.
TestResult runNetworkTests() {
    return runTestGroup(RegisterTest::Network);
}
