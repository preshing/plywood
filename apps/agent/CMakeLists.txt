#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Runtime Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

cmake_minimum_required(VERSION 3.12)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
project(agent)
include(../../share/common.cmake)

# Add plywood
add_plywood(ply-base.* ply-tokenizer.* ply-json.* ply-markdown.* ply-network.* ply-reflect.* ply-agent*.*)
target_compile_definitions(plywood PUBLIC PLY_WITH_HTTP_CLIENT=1 PLY_WITH_HTTP_SERVER=1)

# Add curl
add_curl()
target_link_libraries(plywood PUBLIC curl)

# Add agent
add_source_files(AGENT_SOURCES "${CMAKE_CURRENT_LIST_DIR}" main.cpp)
add_executable(agent ${AGENT_SOURCES})
set_runtime_output_directory(agent "${PLYWOOD_ROOT}/bin")
target_link_libraries(agent PUBLIC plywood)
if(WIN32)
    target_link_libraries(agent PRIVATE ws2_32.lib)
endif()
add_curl_dll_post_build_step(agent)
