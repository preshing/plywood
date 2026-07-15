#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Base Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.12)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(agent-cli)

# Determine PLYWOOD_ROOT
get_filename_component(PLYWOOD_ROOT ../.. REALPATH)

# Load Plywood's common.cmake
include("${PLYWOOD_ROOT}/src/common.cmake")

# plywood
add_source_files(PLYWOOD_SOURCES "${PLYWOOD_ROOT}/src"
                 ply-base.* ply-tokenizer.* ply-json.* ply-markdown.* ply-network.* ply-http.* ply-reflect.*
                 ply-agent.*)
if(WIN32)
    add_source_files(PLYWOOD_SOURCES "${PLYWOOD_ROOT}/src" *.natvis)
endif()
add_library(plywood ${PLYWOOD_SOURCES})
target_compile_definitions(plywood PUBLIC PLY_WITH_HTTP_CLIENT=1 PLY_WITH_HTTP_SERVER=1)
target_include_directories(plywood PUBLIC "${PLYWOOD_ROOT}/src")

# curl
add_curl()
target_link_libraries(plywood PUBLIC curl)

# agent-cli
add_source_files(AGENT_CLI_SOURCES "${CMAKE_CURRENT_LIST_DIR}" main.cpp)
add_executable(agent-cli ${AGENT_CLI_SOURCES})
target_link_libraries(agent-cli PUBLIC plywood)
add_curl_dll_post_build_step(agent-cli)
if(WIN32)
    target_link_libraries(agent-cli PRIVATE ws2_32.lib)
endif()
