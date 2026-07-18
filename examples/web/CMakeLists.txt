# Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
# SPDX-License-Identifier: MIT

# WebAssembly demo of the opal player. Builds only under Emscripten.

set(PLAYER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../player/src)

add_executable(player_web
    src/web.c
    ${PLAYER_SRC}/format_dro.c
    ${PLAYER_SRC}/format_hsc.c
    ${PLAYER_SRC}/format_imf.c
)
target_include_directories(player_web PRIVATE ${PLAYER_SRC})
target_link_libraries(player_web PRIVATE opal miniaudio)

set_target_properties(player_web PROPERTIES
    OUTPUT_NAME player
    SUFFIX ".js"
    FOLDER "examples"
)
target_link_options(player_web PRIVATE
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createPlayer"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sENVIRONMENT=web"
    "-sEXPORTED_FUNCTIONS=_webInit,_webLoad,_webStop,_webFinished,_webLevels,_malloc,_free"
    "-sEXPORTED_RUNTIME_METHODS=ccall,HEAPU8"
)
