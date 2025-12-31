#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Base Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(webserver)
include(../../src/common.cmake)

# plywood
add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" dlmalloc.c ply-base.* ply-network.*)
if(WIN32)
    add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" *.natvis)
endif()
add_library(plywood ${PLYWOOD_SOURCES})
target_include_directories(plywood PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../src")

# webserver
add_source_files(WEBSERVER_SOURCES "${CMAKE_CURRENT_LIST_DIR}" webserver.cpp)
add_executable(webserver ${WEBSERVER_SOURCES})
target_link_libraries(webserver PRIVATE plywood)
if(WIN32)
    target_link_libraries(webserver PRIVATE ws2_32.lib)
endif()
set(PLYWOOD_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
cmake_path(NORMAL_PATH PLYWOOD_ROOT_DIR)
target_compile_definitions(webserver PRIVATE
                           PLYWOOD_ROOT_DIR="${PLYWOOD_ROOT_DIR}")
