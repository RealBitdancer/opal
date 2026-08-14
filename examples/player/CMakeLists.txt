# Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
# SPDX-License-Identifier: MIT

add_executable(player
    src/player.c
    src/player_format.h
    src/format_dro.c
    src/format_dro.h
    src/format_hsc.c
    src/format_hsc.h
    src/format_imf.c
    src/format_imf.h
)
target_link_libraries(player PRIVATE opal miniaudio)
set_target_properties(player PROPERTIES FOLDER "examples")
opal_strict_warnings(player)
if(MSVC)
    target_compile_definitions(player PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
