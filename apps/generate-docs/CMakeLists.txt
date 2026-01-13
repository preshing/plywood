#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Base Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(generate-docs)
include(../../src/common.cmake)

# plywood
add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" dlmalloc.c ply-markdown.* ply-tokenizer.* ply-cpp.* ply-json.* ply-math.* ply-base.*)
if(WIN32)
    add_source_files(PLYWOOD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../src" *.natvis)
endif()
add_library(plywood ${PLYWOOD_SOURCES})
target_include_directories(plywood PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../src")
target_compile_definitions(plywood PUBLIC PLY_WITH_DIRECTORY_WATCHER=1)
if(APPLE AND NOT IOS)
    target_link_libraries(plywood PUBLIC "-framework CoreServices")  # For DirectoryWatcher
endif()

# generate-docs
add_source_files(GENERATE_DOCS_SOURCES "${CMAKE_CURRENT_LIST_DIR}" generate-docs.cpp)
add_executable(generate-docs ${GENERATE_DOCS_SOURCES})
target_link_libraries(generate-docs PRIVATE plywood)
set(PLYWOOD_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
cmake_path(NORMAL_PATH PLYWOOD_ROOT_DIR)
target_compile_definitions(generate-docs PRIVATE
                           PLYWOOD_ROOT_DIR="${PLYWOOD_ROOT_DIR}")
