#──────────────────────────────────────────────────────────┐
#     ____                                                 │
#    ╱   ╱╲     Plywood C++ Runtime Library                │
#   ╱___╱╭╮╲    https://plywood.dev/                       │
#    └──┴┴┴┘                                               │
#──────────────────────────────────────────────────────────┘

cmake_minimum_required(VERSION 3.12)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(agent-proxy)
include(../../share/common.cmake)

# Add Plywood with HTTP client and server support.
add_plywood(ply-system.* ply-tokenizer.* ply-json.* ply-network.* ply-reflect.*)
target_compile_definitions(plywood PUBLIC PLY_WITH_HTTP_CLIENT=1 PLY_WITH_HTTP_SERVER=1)

# Add curl.
add_curl()
target_link_libraries(plywood PUBLIC curl)

# Add agent-proxy.
add_source_files(AGENT_PROXY_SOURCES "${CMAKE_CURRENT_LIST_DIR}" main.cpp)
add_executable(agent-proxy ${AGENT_PROXY_SOURCES})
set_runtime_output_directory(agent-proxy "${PLYWOOD_ROOT}/bin")
target_link_libraries(agent-proxy PUBLIC plywood)
if(WIN32)
    target_link_libraries(agent-proxy PRIVATE ws2_32.lib)
endif()
add_curl_dll_post_build_step(agent-proxy)

# Copy the shared known provider configuration next to the executable.
set(KNOWN_PROVIDERS_PATH "${CMAKE_CURRENT_LIST_DIR}/../agent/known-providers.json")
set_property(TARGET agent-proxy APPEND PROPERTY LINK_DEPENDS "${KNOWN_PROVIDERS_PATH}")
add_custom_command(TARGET agent-proxy POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                   "${KNOWN_PROVIDERS_PATH}"
                   "$<TARGET_FILE_DIR:agent-proxy>/known-providers.json")
