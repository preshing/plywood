#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Runtime Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
project(make-banner-comment)
include(../../src/common.cmake)

# Add plywood
add_plywood(ply-base.*)

# Add make-banner-comment
add_source_files(MAKE_BANNER_COMMENT_SOURCES "${CMAKE_CURRENT_LIST_DIR}" make-banner-comment.cpp)
add_executable(make-banner-comment ${MAKE_BANNER_COMMENT_SOURCES})
set_runtime_output_directory(make-banner-comment "${PLYWOOD_ROOT}/bin")
target_link_libraries(make-banner-comment PRIVATE plywood)
