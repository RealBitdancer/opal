# Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
# SPDX-License-Identifier: MIT

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Shared code used by examples. Audio backend etc.
add_subdirectory(common/miniaudio)

if(EMSCRIPTEN)
    add_subdirectory(web)
else()
    add_subdirectory(player)

    add_custom_target(player_web SOURCES
        web/src/web.c
        web/index.html
        web/CMakeLists.txt
    )
    set_target_properties(player_web PROPERTIES FOLDER "examples")
endif()
