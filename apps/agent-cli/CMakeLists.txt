#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Base Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.12)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(agent-cli)
include(../../src/common.cmake)

# Add plywood
add_plywood(ply-base.* ply-tokenizer.* ply-json.* ply-markdown.* ply-network.* ply-http.* ply-reflect.* ply-agent.*)
target_compile_definitions(plywood PUBLIC PLY_WITH_HTTP_CLIENT=1 PLY_WITH_HTTP_SERVER=1)

# Add curl
add_curl()
target_link_libraries(plywood PUBLIC curl)

# Add agent-cli
add_source_files(AGENT_CLI_SOURCES "${CMAKE_CURRENT_LIST_DIR}" main.cpp)
add_executable(agent-cli ${AGENT_CLI_SOURCES})
target_link_libraries(agent-cli PUBLIC plywood)
add_curl_dll_post_build_step(agent-cli)
if(WIN32)
    target_link_libraries(agent-cli PRIVATE ws2_32.lib)
endif()
