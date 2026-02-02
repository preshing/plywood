/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
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
#if PLY_USE_DLMALLOC
        auto beginStats = getHeapStats();
#endif
        testCases[i].func();
#if PLY_USE_DLMALLOC
        // Check for memory leaks
        auto endStats = getHeapStats();
        if (beginStats.inUseBytes != endStats.inUseBytes) {
            gTestState.success = false;
        }
#endif
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
