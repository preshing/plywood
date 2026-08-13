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
project(generate-docs)
include(../../share/common.cmake)

# Add plywood
add_plywood(ply-markdown.* ply-tokenizer.* ply-cpp.* ply-math.* ply-system.*)
if(WIN32)
    target_compile_definitions(plywood PUBLIC PLY_WITH_DIRECTORY_WATCHER=1)
elseif(APPLE)
    target_compile_definitions(plywood PUBLIC PLY_WITH_DIRECTORY_WATCHER=1)
    target_link_libraries(plywood PUBLIC "-framework CoreServices")
endif()

# Add generate-docs
add_source_files(GENERATE_DOCS_SOURCES "${CMAKE_CURRENT_LIST_DIR}" generate-docs.cpp)
add_executable(generate-docs ${GENERATE_DOCS_SOURCES})
set_runtime_output_directory(generate-docs "${PLYWOOD_ROOT}/bin")
target_link_libraries(generate-docs PRIVATE plywood)
set(PLYWOOD_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
cmake_path(NORMAL_PATH PLYWOOD_ROOT_DIR)
target_compile_definitions(generate-docs PRIVATE
                           PLYWOOD_ROOT_DIR="${PLYWOOD_ROOT_DIR}")
