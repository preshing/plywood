/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include "test-suite.h"

struct Case {
    StringView name;
    void (*func)();
};

Array<Case>& getTestCases() {
    static Array<Case> cases;
    return cases;
}

RegisterTest::RegisterTest(StringView name, void (*func)()) {
    getTestCases().append({name, func});
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

int main() {
    u32 numPassed = 0;
    const auto& testCases = getTestCases();
    Stream out = getStdOut();

    for (u32 i = 0; i < testCases.numItems(); i++) {
        out.format("[{}/{}] {}... ", (i + 1), testCases.numItems(), testCases[i].name);
        gTestState.success = true;

        // Check for memory leaks while running the test case.
        Heap::Stats beginStats = Heap::getStats();
        testCases[i].func();
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
    if (testCases.numItems() > 0) {
        frac = (float) numPassed / testCases.numItems();
    }
    out.format("{}/{} test cases passed ({}%)\n", numPassed, testCases.numItems(), frac * 100.f);

    return (numPassed == testCases.numItems()) ? 0 : 1;
}
