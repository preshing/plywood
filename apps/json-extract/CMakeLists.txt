#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Runtime Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(json-extract)
include(../../src/common.cmake)

# Add plywood
add_plywood(ply-json.* ply-tokenizer.* ply-base.*)

# Add json-extract
add_source_files(JSON_EXTRACT_SOURCES "${CMAKE_CURRENT_LIST_DIR}" json-extract.cpp)
add_executable(json-extract ${JSON_EXTRACT_SOURCES})
target_link_libraries(json-extract PRIVATE plywood)
