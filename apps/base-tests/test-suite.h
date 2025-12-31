/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include <ply-base.h>

using namespace ply;

struct RegisterTest {
    RegisterTest(StringView name, void (*func)());
};

#define TEST_CASE(name) \
    void PLY_CAT(PLY_CAT(test_, TEST_CASE_PREFIX), __LINE__)(); \
    void (*PLY_CAT(PLY_CAT(testlink_, TEST_CASE_PREFIX), __LINE__))() = \
        &PLY_CAT(PLY_CAT(test_, TEST_CASE_PREFIX), __LINE__); \
    RegisterTest PLY_CAT(PLY_CAT(autoReg_, TEST_CASE_PREFIX), __LINE__){ \
        name, PLY_CAT(PLY_CAT(test_, TEST_CASE_PREFIX), __LINE__)}; \
    void PLY_CAT(PLY_CAT(test_, TEST_CASE_PREFIX), __LINE__)()

bool check(bool);
