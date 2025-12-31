#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Base Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(markdown-tests)
include(../../src/common.cmake)

# plywood
add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" dlmalloc.c ply-json.* ply-tokenizer.* ply-markdown.* ply-math.* ply-base.*)
if(WIN32)
    add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" *.natvis)
endif()
add_library(plywood ${PLYWOOD_SOURCES})
target_include_directories(plywood PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../src")

# markdown-tests
add_source_files(MARKDOWN_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}" markdown-tests.cpp)
add_executable(markdown-tests ${MARKDOWN_TEST_SOURCES})
target_link_libraries(markdown-tests PRIVATE plywood)
target_compile_definitions(markdown-tests PRIVATE MARKDOWN_TESTS_PATH="${CMAKE_CURRENT_LIST_DIR}")
