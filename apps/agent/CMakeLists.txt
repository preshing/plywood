#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Runtime Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.12)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(agent)
include(../../src/common.cmake)

# Add plywood
add_plywood(ply-base.* ply-tokenizer.* ply-json.* ply-markdown.* ply-network.* ply-http.* ply-reflect.* ply-agent.*
            ply-agent-tools.*)
target_compile_definitions(plywood PUBLIC PLY_WITH_HTTP_CLIENT=1 PLY_WITH_HTTP_SERVER=1)

# Add curl
add_curl()
target_link_libraries(plywood PUBLIC curl)

# Add agent
add_source_files(AGENT_SOURCES "${CMAKE_CURRENT_LIST_DIR}" main.cpp)
add_executable(agent ${AGENT_SOURCES})
target_link_libraries(agent PUBLIC plywood)
add_curl_dll_post_build_step(agent)
if(WIN32)
    target_link_libraries(agent PRIVATE ws2_32.lib)
endif()
