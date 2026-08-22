/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

#ifndef PLAYER_FORMAT_H_INCLUDED
#define PLAYER_FORMAT_H_INCLUDED

#include "opal/opal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STEP_STALL_LIMIT 1024

typedef struct MusicSource MusicSource;

struct MusicSource
{
    // frames until next event. 0 and !*done means step again
    uint64_t (*step)(MusicSource* self, Opal* opl, bool* done);
    void (*free)(MusicSource* self);
    bool loop;
};

// NULL if unmatched. Borrows data. May program opl.
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
