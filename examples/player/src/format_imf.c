/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

// IMF decoder. Raw OPL2 register log from id and Apogee titles. Body is 4 byte units:
// reg, val, u16 delay (LE) after the write.
//
// Type 0: raw to EOF, often starts with reg 0x00=0x00.
// Type 1: prefixed by u16 length. Nonzero first word marks it.
//
// Rate is not in the file. Chosen by extension: .wlf 700 Hz, .imf 560 Hz. See player_format.h.

#include "format_imf.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
    MusicSource base;
    const uint8_t* data; // command stream (reg, val, u16 delay) units, borrowed from the file buffer
    size_t size;
    size_t pos;
    uint32_t sampleRate;
    uint32_t rate; // IMF tick rate in Hz (280 / 560 / 700)
    uint32_t frac; // fractional-frame carry so delays do not drift
} ImfSource;

static uint32_t g_rateOverride; // 0 = pick by extension; otherwise force this rate (Hz)
static uint32_t g_lastRate;     // rate the most recent imfLoad settled on, for the caller to report

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
        return 0; // more writes now, caller steps again
    }

    // ticks at s->rate to frames. Carry frac so tempo holds.
    uint64_t num = (uint64_t)delay * s->sampleRate + s->frac;
    s->frac = (uint32_t)(num % s->rate);
    return num / s->rate;
}

static void imfFree(MusicSource* base)
{
    free(base);
}

// 560 Hz for .imf, 700 Hz for .wlf, 0 if the extension is neither (IMF has no signature to probe).
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
    (void)opl; // the command stream programs the chip itself

    // Extension gate first. Override only affects replay rate of recognized files.
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

    // Nonzero first word: type1 length prefix. Clamp to avail, drop trailing tags.
    // Zero first word: type0, whole file is stream.
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
    s->data = stream;
    s->size = streamSize;
    s->sampleRate = sampleRate;
    s->rate = rate;
    return &s->base;
}

const MusicFormat imfFormat = {"IMF", imfLoad};
