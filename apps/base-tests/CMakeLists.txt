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

# plywood
add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" dlmalloc.c ply-base.* ply-btree.h ply-math.*)
if(WIN32)
    add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" *.natvis)
endif()
add_library(plywood ${PLYWOOD_SOURCES})
target_include_directories(plywood PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../src")

# test-suite
add_source_files(BASE_LIBRARY_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}" test-*.cpp)
add_executable(base-tests ${BASE_LIBRARY_TEST_SOURCES})
target_link_libraries(base-tests PRIVATE plywood)
target_compile_definitions(base-tests PRIVATE BASE_LIBRARY_TESTS_PATH="${CMAKE_CURRENT_LIST_DIR}" BUILD_DIR="${CMAKE_BINARY_DIR}")
