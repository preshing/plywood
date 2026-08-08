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
project(serve-docs)
include(../../share/common.cmake)

# Add plywood
add_plywood(ply-system.* ply-network.*)
target_compile_definitions(plywood PUBLIC PLY_WITH_HTTP_SERVER=1)

# Add serve-docs
add_source_files(SERVE_DOCS_SOURCES "${CMAKE_CURRENT_LIST_DIR}" serve-docs.cpp)
add_executable(serve-docs ${SERVE_DOCS_SOURCES})
set_runtime_output_directory(serve-docs "${PLYWOOD_ROOT}/bin")
target_link_libraries(serve-docs PRIVATE plywood)
if(WIN32)
    target_link_libraries(serve-docs PRIVATE ws2_32.lib)
endif()
set(PLYWOOD_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
cmake_path(NORMAL_PATH PLYWOOD_ROOT_DIR)
target_compile_definitions(serve-docs PRIVATE
                           PLYWOOD_ROOT_DIR="${PLYWOOD_ROOT_DIR}")
