#=========================================================
#      ____
#     ╱   ╱╲    Plywood C++ Base Library
#    ╱___╱╭╮╲   https://plywood.dev/
#     └──┴┴┴┘
#=========================================================

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")
set(CMAKE_STATIC_LINKER_FLAGS "")
set(CMAKE_SHARED_LINKER_FLAGS "")
set(CMAKE_EXE_LINKER_FLAGS "")

foreach(config_name ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config_name} suffix)
    set(CMAKE_C_FLAGS_${suffix} "")
    set(CMAKE_CXX_FLAGS_${suffix} "")
    set(CMAKE_STATIC_LINKER_FLAGS_${suffix} "")
    set(CMAKE_SHARED_LINKER_FLAGS_${suffix} "")
    set(CMAKE_EXE_LINKER_FLAGS_${suffix} "")
endforeach()

if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG /INCREMENTAL /EDITANDCONTINUE /LTCG:OFF /SAFESEH:NO")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG /INCREMENTAL:NO")
    set(CMAKE_EXE_LINKER_FLAGS_FINAL "/DEBUG /INCREMENTAL:NO")
    set(CMAKE_C_FLAGS_DEBUG "/D_DEBUG /DPLY_WITH_ASSERTS=1 /MTd /ZI /Od /Ob0 /RTC1")
    set(CMAKE_C_FLAGS_RELEASE "/DNDEBUG /DPLY_WITH_ASSERTS=1 /MT /Zi /O2 /Ob1 /Oi")
    set(CMAKE_C_FLAGS_FINAL "/DNDEBUG /MT /Zi /O2 /Ob1 /Oi")
else()
    set(CMAKE_C_FLAGS "-g -Wall -fno-stack-protector -pthread")
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wno-invalid-offsetof --std=c++14")
    set(CMAKE_C_FLAGS_DEBUG "-DPLY_WITH_ASSERTS=1 -D_DEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-DPLY_WITH_ASSERTS=1 -Os")
    set(CMAKE_C_FLAGS_FINAL "-Os")
endif()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_FINAL "${CMAKE_C_FLAGS_FINAL}")

if(IOS)
    if(DEFINED PLY_XCODE_DEVELOPMENT_TEAM)
        set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${PLY_XCODE_DEVELOPMENT_TEAM}" CACHE STRING "Xcode Development Team" FORCE)
    elseif(DEFINED ENV{PLY_XCODE_DEVELOPMENT_TEAM})
        set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "$ENV{PLY_XCODE_DEVELOPMENT_TEAM}" CACHE STRING "Xcode Development Team")
    elseif(NOT DEFINED CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM)
        message(SEND_ERROR "PLY_XCODE_DEVELOPMENT_TEAM must be specified on the command line or in an environment variable")
    endif()
elseif(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.6")
endif()

#--------------------------------------
# add_source_files
#--------------------------------------
function(add_source_files var_name root_path)
    set(root_ide_folder "")
    set(directive "")
    set(glob_type GLOB)
    foreach(arg ${ARGN})
        if(directive STREQUAL "IDE_FOLDER")
            set(root_ide_folder "${arg}")
            set(directive "")
        elseif(arg STREQUAL "IDE_FOLDER")
            set(directive "${arg}")
        elseif(arg STREQUAL "RECURSIVE")
            set(glob_type GLOB_RECURSE)
        else()
            list(APPEND glob_patterns "${arg}")
        endif()
    endforeach()
    if(directive STREQUAL "IDE_FOLDER")
        message(FATAL_ERROR "Expected argument after \"${directive}\"")
    endif()
    foreach(glob ${glob_patterns})
        file(${glob_type} file_list RELATIVE "${root_path}" "${root_path}/${glob}")
        if(NOT file_list)
            message(WARNING "No files matching \"${root_path}/${glob}\"")
        endif()
        foreach(rel_file ${file_list})
            # Exclude files that start with .
            if(NOT rel_file MATCHES "^\\..*")
                list(APPEND ${var_name} "${root_path}/${rel_file}")
                get_filename_component(folder "${rel_file}" PATH)
                string(REPLACE / \\ ide_folder "${root_ide_folder}/${folder}")
                source_group("${ide_folder}" FILES "${root_path}/${rel_file}")
            endif()
        endforeach()
    endforeach()
    set(${var_name} "${${var_name}}" PARENT_SCOPE)
endfunction()

#--------------------------------------
# curl support
#--------------------------------------
function(add_curl)
    if(WIN32)
        # Curl must be installed using: vcpkg install curl[openssl]:x64-windows
        add_library(curl INTERFACE)
        if (NOT DEFINED VCPKG_PATH)
            get_filename_component(VCPKG_PATH "${PLYWOOD_ROOT}/../vcpkg" REALPATH)
        endif()
        file(TO_NATIVE_PATH "${VCPKG_PATH}" VCPKG_NATIVE_PATH)
        if (NOT EXISTS "${VCPKG_PATH}/vcpkg.exe")
            message("*** Could not locate vcpkg at: ${VCPKG_NATIVE_PATH} ***")
            message("Pass -DVCPKG_PATH=<path> to the cmake command line to specify an alternative path.")
            message("Otherwise, vcpkg can be installed using: git clone https://github.com/microsoft/vcpkg.git \"${VCPKG_NATIVE_PATH}\"")
            message("Then run: \"${VCPKG_NATIVE_PATH}\\bootstrap-vcpkg.bat\"")
            message(FATAL_ERROR "Could not locate vcpkg!")
        endif()
        set(VCPKG_WIN64_PATH "${VCPKG_PATH}/installed/x64-windows")
        if (NOT EXISTS "${VCPKG_WIN64_PATH}/lib/libcurl.lib")
            message("*** Could not locate: ${VCPKG_NATIVE_PATH}\\installed\\x64-windows\\lib\\libcurl.lib ***")
            message("Install it using: \"${VCPKG_NATIVE_PATH}\\vcpkg\" install curl[openssl]:x64-windows")
            message(FATAL_ERROR "Could not locate libcurl.lib!")
        endif()
        target_include_directories(curl INTERFACE "${VCPKG_WIN64_PATH}/include")
        target_link_libraries(curl INTERFACE
            "${VCPKG_WIN64_PATH}/lib/libcurl.lib"
            "${VCPKG_WIN64_PATH}/lib/libcrypto.lib"
            "${VCPKG_WIN64_PATH}/lib/libssl.lib")
        set(VCPKG_PATH "${VCPKG_PATH}" CACHE PATH "Path to vcpkg.exe" FORCE)
        set(VCPKG_WIN64_PATH "${VCPKG_WIN64_PATH}" PARENT_SCOPE)
    endif()
endfunction()

function(add_curl_dll_post_build_step target_name)
    if(WIN32)
        add_custom_command(TARGET "${target_name}" POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_WIN64_PATH}/bin/libcurl.dll"
            "${VCPKG_WIN64_PATH}/bin/libssl-3-x64.dll"
            "${VCPKG_WIN64_PATH}/bin/libcrypto-3-x64.dll"
            "${VCPKG_WIN64_PATH}/bin/z.dll"
            "${PLYWOOD_ROOT}/src/cacert.pem"
            $<TARGET_FILE_DIR:${target_name}>)
    endif()
endfunction()
