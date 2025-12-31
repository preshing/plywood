#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Base Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(fix-case)
include(../../src/common.cmake)

# plywood
add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" dlmalloc.c ply-base.* ply-tokenizer.*)
if(WIN32)
    add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" *.natvis)
endif()
add_library(plywood ${PLYWOOD_SOURCES})
target_include_directories(plywood PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../src")

# test-suite
add_source_files(FIX_CASE_SOURCES "${CMAKE_CURRENT_LIST_DIR}" fix-case.cpp)
add_executable(fix-case ${FIX_CASE_SOURCES})
target_link_libraries(fix-case PRIVATE plywood)
set(PLYWOOD_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
cmake_path(NORMAL_PATH PLYWOOD_ROOT_DIR)
target_compile_definitions(fix-case PRIVATE PLYWOOD_ROOT_DIR="${PLYWOOD_ROOT_DIR}")
