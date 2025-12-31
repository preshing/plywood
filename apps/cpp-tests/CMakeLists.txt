#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Base Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(cpp-tests)
include(../../src/common.cmake)

# plywood
add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" dlmalloc.c ply-base.* ply-tokenizer.* ply-cpp.*)
if(WIN32)
    add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" *.natvis)
endif()
add_library(plywood ${PLYWOOD_SOURCES})
target_include_directories(plywood PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../src")

# cpp-tests
add_source_files(CPP_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}" cpp-tests.cpp)
add_executable(cpp-tests ${CPP_TEST_SOURCES})
target_link_libraries(cpp-tests PRIVATE plywood)
target_compile_definitions(cpp-tests PRIVATE CPP_TESTS_PATH="${CMAKE_CURRENT_LIST_DIR}")
