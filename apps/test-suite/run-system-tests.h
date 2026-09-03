/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    test-suite                                        │
│               Documentation: docs/apps/test-suite.md            │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

#include "test-suite.h"

using namespace ply;

struct RegisterTest {
    enum Group {
        System,
        Network,
        UnicodeLoading,
    };

    RegisterTest(StringView name, void (*func)(), Group group = System);
};

#define TEST_CASE_IN_GROUP(name, group) \
    void PLY_CAT(PLY_CAT(test_, TEST_CASE_PREFIX), __LINE__)(); \
    void (*PLY_CAT(PLY_CAT(testlink_, TEST_CASE_PREFIX), __LINE__))() = \
        &PLY_CAT(PLY_CAT(test_, TEST_CASE_PREFIX), __LINE__); \
    RegisterTest PLY_CAT(PLY_CAT(autoReg_, TEST_CASE_PREFIX), \
                         __LINE__){name, PLY_CAT(PLY_CAT(test_, TEST_CASE_PREFIX), __LINE__), group}; \
    void PLY_CAT(PLY_CAT(test_, TEST_CASE_PREFIX), __LINE__)()

#define TEST_CASE(name) TEST_CASE_IN_GROUP(name, RegisterTest::System)
#define NETWORK_TEST_CASE(name) TEST_CASE_IN_GROUP(name, RegisterTest::Network)
#define UNICODE_LOADING_TEST_CASE(name) TEST_CASE_IN_GROUP(name, RegisterTest::UnicodeLoading)

bool check(bool);
