# Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
# SPDX-License-Identifier: MIT

set(PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(miniaudio STATIC)

target_sources(miniaudio
    PRIVATE
        src/miniaudio.c
)

target_sources(miniaudio
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${PUBLIC_INCLUDE_DIR}
    FILES ${PUBLIC_INCLUDE_DIR}/miniaudio/miniaudio.h
)

set_target_properties(miniaudio PROPERTIES FOLDER "examples/common")

target_compile_features(miniaudio PRIVATE c_std_11)
if(MSVC)
    target_compile_options(miniaudio PRIVATE /W0)
else()
    target_compile_options(miniaudio PRIVATE -w)
endif()

# Skip Threads on Emscripten: -pthread would force COOP/COEP on the host.
if(UNIX AND NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    target_link_libraries(miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
