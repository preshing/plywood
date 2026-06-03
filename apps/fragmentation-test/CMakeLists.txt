#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Base Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(fragmentation-test)
include(../../src/common.cmake)

# plywood
add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" ply-base.*)
if(WIN32)
    add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" *.natvis)
endif()
add_library(plywood ${PLYWOOD_SOURCES})
target_include_directories(plywood PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../src")

# fragmentation-test
add_source_files(HEAP_STRESS_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}" fragmentation-test.cpp)
add_executable(fragmentation-test ${HEAP_STRESS_TEST_SOURCES})
target_link_libraries(fragmentation-test PRIVATE plywood)
