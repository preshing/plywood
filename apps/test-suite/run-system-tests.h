/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-system.h>

using namespace ply;

struct RegisterTest {
    enum Group {
        System,
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
#define UNICODE_LOADING_TEST_CASE(name) TEST_CASE_IN_GROUP(name, RegisterTest::UnicodeLoading)

bool check(bool);

bool runSystemTests();
bool runUnicodeLoadingTests();
