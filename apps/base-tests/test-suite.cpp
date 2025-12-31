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

Array<Case>& get_test_cases() {
    static Array<Case> cases;
    return cases;
}

RegisterTest::RegisterTest(StringView name, void (*func)()) {
    get_test_cases().append({name, func});
}

struct TestState {
    bool success = true;
};

TestState g_test_state;

bool check(bool cond) {
    if (!cond) {
        g_test_state.success = false;
    }
    return cond;
}

int main() {
    u32 num_passed = 0;
    const auto& test_cases = get_test_cases();
    Stream out = get_stdout();

    for (u32 i = 0; i < test_cases.num_items(); i++) {
        out.format("[{}/{}] {}... ", (i + 1), test_cases.num_items(), test_cases[i].name);
        g_test_state.success = true;
#if PLY_USE_DLMALLOC
        auto begin_stats = get_heap_stats();
#endif
        test_cases[i].func();
#if PLY_USE_DLMALLOC
        // Check for memory leaks
        auto end_stats = get_heap_stats();
        if (begin_stats.in_use_bytes != end_stats.in_use_bytes) {
            g_test_state.success = false;
        }
#endif
        out.write(g_test_state.success ? "success\n" : "***FAIL***\n");
        if (g_test_state.success) {
            num_passed++;
        }
        out.flush();
    }
    float frac = 1.f;
    if (test_cases.num_items() > 0) {
        frac = (float) num_passed / test_cases.num_items();
    }
    out.format("{}/{} test cases passed ({}%)\n", num_passed, test_cases.num_items(), frac * 100.f);

    return (num_passed == test_cases.num_items()) ? 0 : 1;
}
