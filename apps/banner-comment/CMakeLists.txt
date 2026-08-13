#──────────────────────────────────────────────────────────┐
#     ____                                                 │
#    ╱   ╱╲     Plywood C++ Runtime Library                │
#   ╱___╱╭╮╲    https://plywood.dev/                       │
#    └──┴┴┴┘                                               │
#──────────────────────────────────────────────────────────┘

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
project(banner-comment)
include(../../share/common.cmake)

# Add plywood
add_plywood(ply-system.*)

# Add banner-comment
add_source_files(BANNER_COMMENT_SOURCES "${CMAKE_CURRENT_LIST_DIR}" banner-comment.cpp)
add_executable(banner-comment ${BANNER_COMMENT_SOURCES})
set_runtime_output_directory(banner-comment "${PLYWOOD_ROOT}/bin")
target_link_libraries(banner-comment PRIVATE plywood)
