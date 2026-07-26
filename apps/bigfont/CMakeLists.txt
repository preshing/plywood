#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Runtime Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(bigfont)
include(../../src/common.cmake)

# Add plywood
add_plywood(ply-base.*)

# Add bigfont
add_source_files(BIGFONT_SOURCES "${CMAKE_CURRENT_LIST_DIR}" bigfont.cpp)
add_executable(bigfont ${BIGFONT_SOURCES})
target_link_libraries(bigfont PRIVATE plywood)
