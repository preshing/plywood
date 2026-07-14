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

# agent-cli
add_source_files(AGENT_CLI_SOURCES "${CMAKE_CURRENT_LIST_DIR}" main.cpp)
add_executable(agent-cli ${AGENT_CLI_SOURCES})
target_link_libraries(agent-cli PUBLIC plywood)
if(WIN32)
    target_link_libraries(agent-cli PRIVATE ws2_32.lib)
endif()

# Add curl to plywood
if(WIN32)
    # Curl was installed using: vcpkg install curl[openssl]:x64-windows
    if (NOT DEFINED VCPKG_PATH)
        get_filename_component(VCPKG_PATH "${PLYWOOD_ROOT}/../vcpkg" REALPATH)
    endif()
    get_filename_component(VCPKG_WIN64_PATH "${VCPKG_PATH}/installed/x64-windows" REALPATH)
    target_include_directories(plywood PUBLIC "${VCPKG_WIN64_PATH}/include")
    target_link_libraries(plywood PUBLIC
        "${VCPKG_WIN64_PATH}/lib/libcurl.lib"
        "${VCPKG_WIN64_PATH}/lib/libcrypto.lib"
        "${VCPKG_WIN64_PATH}/lib/libssl.lib")
    add_custom_command(TARGET agent-cli POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VCPKG_WIN64_PATH}/bin/libcurl.dll"
        "${VCPKG_WIN64_PATH}/bin/libssl-3-x64.dll"
        "${VCPKG_WIN64_PATH}/bin/libcrypto-3-x64.dll"
        "${VCPKG_WIN64_PATH}/bin/zlib1.dll"
        "${PLYWOOD_ROOT}/src/cacert.pem"
        $<TARGET_FILE_DIR:agent-cli>)
endif()
