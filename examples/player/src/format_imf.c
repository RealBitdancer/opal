/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

#include "format_imf.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
    MusicSource base;
    const uint8_t* data;
    size_t size;
    size_t pos;
    uint32_t sampleRate;
    uint32_t rate;
    uint32_t frac;
} ImfSource;

static uint32_t g_rateOverride;
static uint32_t g_lastRate;

void imfSetRate(uint32_t hz)
{
    g_rateOverride = hz;
}

uint32_t imfRate(void)
{
    return g_lastRate;
}

static uint64_t imfStep(MusicSource* base, Opal* opl, bool* done)
{
    ImfSource* s = (ImfSource*)base;

    if (s->pos + 4 > s->size)
    {
        *done = true;
        return 0;
    }

    uint8_t reg = s->data[s->pos];
    uint8_t val = s->data[s->pos + 1];
    uint16_t delay = readU16Le(s->data + s->pos + 2);
    s->pos += 4;

    opalWriteRegBuffered(opl, reg, val);
    if (delay == 0)
    {
        return 0;
    }

    uint64_t num = (uint64_t)delay * s->sampleRate + s->frac;
    s->frac = (uint32_t)(num % s->rate);
    return num / s->rate;
}

static void imfFree(MusicSource* base)
{
    free(base);
}

static uint32_t imfRateForExt(const char* path)
{
    size_t n = strlen(path);
    if (n < 4 || path[n - 4] != '.')
    {
        return 0;
    }
    const char* e = path + n - 3;
    bool imf = (e[0] == 'i' || e[0] == 'I') && (e[1] == 'm' || e[1] == 'M') && (e[2] == 'f' || e[2] == 'F');
    bool wlf = (e[0] == 'w' || e[0] == 'W') && (e[1] == 'l' || e[1] == 'L') && (e[2] == 'f' || e[2] == 'F');
    if (wlf)
    {
        return 700;
    }
    if (imf)
    {
        return 560;
    }
    return 0;
}

static MusicSource* imfLoad(const char* path, const uint8_t* data, size_t size, uint32_t sampleRate, Opal* opl)
{
    (void)opl;

    uint32_t rate = imfRateForExt(path);
    if (rate == 0 || size < 4)
    {
        return NULL;
    }
    if (g_rateOverride != 0)
    {
        rate = g_rateOverride;
    }
    g_lastRate = rate;

    // Nonzero first word is a type-1 length prefix.
    const uint8_t* stream = data;
    size_t streamSize = size;
    uint16_t lenWord = readU16Le(data);
    if (lenWord != 0)
    {
        stream = data + 2;
        size_t avail = size - 2;
        streamSize = lenWord <= avail ? lenWord : avail;
    }

    ImfSource* s = (ImfSource*)calloc(1, sizeof *s);
    if (s == NULL)
    {
        return NULL;
    }

    s->base.step = imfStep;
    s->base.free = imfFree;
    s->base.loop = false;
    s->data = stream;
    s->size = streamSize;
    s->sampleRate = sampleRate;
    s->rate = rate;
    return &s->base;
}

const MusicFormat imfFormat = {"IMF", imfLoad};
