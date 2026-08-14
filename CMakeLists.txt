# Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.23)

cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# Keep in lockstep with OPAL_VERSION_* in include/opal/opal.h
project(opal VERSION 2.0.2 LANGUAGES C)

include(GNUInstallDirs)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Limited configurations" FORCE)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT opalIpoOk)
    if(opalIpoOk)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    endif()
endif()

function(opal_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

# GCC -flto=auto defers LTRANS count to jobserver. Ninja has none so GCC falls back to serial
# and warns. -flto-partition=none forces single partition to avoid the warning. MSVC and Clang
# handle LTO their own way.
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_link_options($<$<CONFIG:Release>:-flto-partition=none>)
endif()

option(OPAL_BUILD_EXAMPLES "Build opal examples" ON)
message(STATUS "OPAL_BUILD_EXAMPLES is set to: ${OPAL_BUILD_EXAMPLES}")

add_subdirectory(src)
if(OPAL_BUILD_EXAMPLES)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT player)
    add_subdirectory(examples)
endif()
