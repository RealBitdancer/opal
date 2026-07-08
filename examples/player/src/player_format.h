/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

// Interface between player core and per format decoders. Each format in its format_*.c exposes
// a MusicFormat. Core probes in order and drives the winner.

#ifndef PLAYER_FORMAT_H_INCLUDED
#define PLAYER_FORMAT_H_INCLUDED

#include "opal/opal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct MusicSource MusicSource;

struct MusicSource
{
    // Apply due writes to opl. Return output frames until next event. 0 with *done=0 means step again now.
    // Set *done true at song end.
    uint64_t (*step)(MusicSource* self, Opal* opl, bool* done);
    void (*free)(MusicSource* self);
};

// Return heap MusicSource if this format matches path+data, else NULL. Borrows data. opl is fresh and
// may be preprogrammed here.
typedef MusicSource* (*MusicLoadFn)(const char* path, const uint8_t* data, size_t size, uint32_t sampleRate, Opal* opl);

typedef struct
{
    const char* name;
    MusicLoadFn load;
} MusicFormat;

static inline uint64_t framesForMs(uint32_t sampleRate, uint32_t ms)
{
    return ((uint64_t)ms * sampleRate + 500) / 1000;
}

static inline uint16_t readU16Le(const uint8_t* p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline uint32_t readU32Le(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#endif // PLAYER_FORMAT_H_INCLUDED
