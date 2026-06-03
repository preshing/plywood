#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Base Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(json-extract)
include(../../src/common.cmake)

# plywood
add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" ply-json.* ply-tokenizer.* ply-base.*)
if(WIN32)
    add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" *.natvis)
endif()
add_library(plywood ${PLYWOOD_SOURCES})
target_include_directories(plywood PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../src")

# json-extract
add_source_files(JSON_EXTRACT_SOURCES "${CMAKE_CURRENT_LIST_DIR}" json-extract.cpp)
add_executable(json-extract ${JSON_EXTRACT_SOURCES})
target_link_libraries(json-extract PRIVATE plywood)
