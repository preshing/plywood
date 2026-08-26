#──────────────────────────────────────────────────────────┐
#     ____                                                 │
#    ╱   ╱╲     Plywood C++ Runtime Library                │
#   ╱___╱╭╮╲    https://plywood.dev/                       │
#    └──┴┴┴┘                                               │
#──────────────────────────────────────────────────────────┘

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Final" CACHE INTERNAL "Build configs")
project(test-suite)
include(../../share/common.cmake)

# Resolve the optional suite directives. Any nonzero directive creates an allowlist;
# otherwise, zero directives create a denylist and unspecified suites stay enabled.
set(TEST_DIRECTIVES
    WITH_SYSTEM_TESTS
    WITH_NETWORK_TESTS
    WITH_UNICODE_LOADING_TESTS
    WITH_MARKDOWN_TESTS
    WITH_CPP_TESTS
    WITH_TRANSCRIPT_TESTS
    WITH_FRAGMENTATION_TEST)
set(HAS_EXPLICIT_TEST_INCLUDES FALSE)
foreach(directive ${TEST_DIRECTIVES})
    if(DEFINED ${directive})
        if(${directive})
            set(HAS_EXPLICIT_TEST_INCLUDES TRUE)
        endif()
    endif()
endforeach()

set(NUM_ENABLED_TEST_SUITES 0)
foreach(directive ${TEST_DIRECTIVES})
    if(HAS_EXPLICIT_TEST_INCLUDES)
        set(enabled 0)
        if(DEFINED ${directive})
            if(${directive})
                set(enabled 1)
            endif()
        endif()
    else()
        set(enabled 1)
        if(DEFINED ${directive})
            if(NOT ${directive})
                set(enabled 0)
            endif()
        endif()
    endif()
    set(${directive}_ENABLED ${enabled})
    if(enabled)
        math(EXPR NUM_ENABLED_TEST_SUITES "${NUM_ENABLED_TEST_SUITES} + 1")
    endif()
endforeach()

if(NUM_ENABLED_TEST_SUITES EQUAL 0)
    message(FATAL_ERROR "test-suite must be built with at least one test suite enabled")
endif()

# Add only the Plywood components required by the enabled suites and command-line parser.
set(PLYWOOD_COMPONENTS ply-system.* ply-json.* ply-math.* ply-reflect.* ply-tokenizer.*)
if(WITH_NETWORK_TESTS_ENABLED)
    list(APPEND PLYWOOD_COMPONENTS ply-network.*)
endif()
if(WITH_MARKDOWN_TESTS_ENABLED)
    list(APPEND PLYWOOD_COMPONENTS ply-markdown.*)
endif()
if(WITH_CPP_TESTS_ENABLED)
    list(APPEND PLYWOOD_COMPONENTS ply-cpp.*)
endif()
if(WITH_TRANSCRIPT_TESTS_ENABLED)
    list(APPEND PLYWOOD_COMPONENTS ply-agent.*)
endif()
list(REMOVE_DUPLICATES PLYWOOD_COMPONENTS)
add_plywood(${PLYWOOD_COMPONENTS})
if(WITH_TRANSCRIPT_TESTS_ENABLED)
    target_compile_definitions(plywood PUBLIC PLY_AGENT_TRANSCRIPT_ONLY=1)
endif()

# Add only the test-suite sources supported by this configuration.
set(TEST_SUITE_SOURCE_PATTERNS main.cpp test-suite.h)
if(WITH_NETWORK_TESTS_ENABLED)
    list(APPEND TEST_SUITE_SOURCE_PATTERNS network-tests.cpp)
endif()
if(WITH_SYSTEM_TESTS_ENABLED OR WITH_UNICODE_LOADING_TESTS_ENABLED)
    list(APPEND TEST_SUITE_SOURCE_PATTERNS system-tests.cpp)
endif()
if(WITH_SYSTEM_TESTS_ENABLED OR WITH_NETWORK_TESTS_ENABLED OR WITH_UNICODE_LOADING_TESTS_ENABLED)
    list(APPEND TEST_SUITE_SOURCE_PATTERNS run-system-tests.cpp run-system-tests.h)
endif()
if(WITH_MARKDOWN_TESTS_ENABLED)
    list(APPEND TEST_SUITE_SOURCE_PATTERNS markdown-tests.cpp)
endif()
if(WITH_CPP_TESTS_ENABLED)
    list(APPEND TEST_SUITE_SOURCE_PATTERNS cpp-tests.cpp)
endif()
if(WITH_TRANSCRIPT_TESTS_ENABLED)
    list(APPEND TEST_SUITE_SOURCE_PATTERNS transcript-tests.cpp)
endif()
if(WITH_FRAGMENTATION_TEST_ENABLED)
    list(APPEND TEST_SUITE_SOURCE_PATTERNS fragmentation-test.cpp)
endif()

add_source_files(TEST_SUITE_SOURCES "${CMAKE_CURRENT_LIST_DIR}" ${TEST_SUITE_SOURCE_PATTERNS})
add_executable(test-suite ${TEST_SUITE_SOURCES})
set_runtime_output_directory(test-suite "${PLYWOOD_ROOT}/bin")
target_link_libraries(test-suite PRIVATE plywood)
if(WIN32 AND WITH_NETWORK_TESTS_ENABLED)
    target_link_libraries(test-suite PRIVATE ws2_32.lib)
endif()
target_compile_definitions(test-suite PRIVATE
    TEST_SUITE_PATH="${CMAKE_CURRENT_LIST_DIR}"
    BUILD_DIR="${CMAKE_BINARY_DIR}"
    WITH_SYSTEM_TESTS=${WITH_SYSTEM_TESTS_ENABLED}
    WITH_NETWORK_TESTS=${WITH_NETWORK_TESTS_ENABLED}
    WITH_UNICODE_LOADING_TESTS=${WITH_UNICODE_LOADING_TESTS_ENABLED}
    WITH_MARKDOWN_TESTS=${WITH_MARKDOWN_TESTS_ENABLED}
    WITH_CPP_TESTS=${WITH_CPP_TESTS_ENABLED}
    WITH_TRANSCRIPT_TESTS=${WITH_TRANSCRIPT_TESTS_ENABLED}
    WITH_FRAGMENTATION_TEST=${WITH_FRAGMENTATION_TEST_ENABLED})
