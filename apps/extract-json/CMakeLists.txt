#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Runtime Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(extract-json)
include(../../src/common.cmake)

# Add plywood
add_plywood(ply-json.* ply-tokenizer.* ply-base.*)

# Add extract-json
add_source_files(EXTRACT_JSON_SOURCES "${CMAKE_CURRENT_LIST_DIR}" extract-json.cpp)
add_executable(extract-json ${EXTRACT_JSON_SOURCES})
set_runtime_output_directory(extract-json "${PLYWOOD_ROOT}/bin")
target_link_libraries(extract-json PRIVATE plywood)
