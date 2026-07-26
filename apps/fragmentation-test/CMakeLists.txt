#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Runtime Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(fragmentation-test)
include(../../src/common.cmake)

# Add plywood
add_plywood(ply-base.*)

# Add fragmentation-test
add_source_files(HEAP_STRESS_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}" fragmentation-test.cpp)
add_executable(fragmentation-test ${HEAP_STRESS_TEST_SOURCES})
target_link_libraries(fragmentation-test PRIVATE plywood)
