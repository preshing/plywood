#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Base Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(base-tests)
include(../../src/common.cmake)

# Add plywood
add_plywood(ply-base.* ply-btree.h ply-math.*)

# Add test-suite
add_source_files(BASE_LIBRARY_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}" test-*.cpp)
add_executable(base-tests ${BASE_LIBRARY_TEST_SOURCES})
target_link_libraries(base-tests PRIVATE plywood)
target_compile_definitions(base-tests PRIVATE BASE_LIBRARY_TESTS_PATH="${CMAKE_CURRENT_LIST_DIR}" BUILD_DIR="${CMAKE_BINARY_DIR}")
