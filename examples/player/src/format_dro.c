/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

// DRO v1 decoder. See player_format.h.

#include "format_dro.h"

#include <stdlib.h>
#include <string.h>

#define DRO_HEADER_SIZE 24

typedef struct
{
    MusicSource base;
    const uint8_t* data; // command stream, borrowed
    size_t size;
    size_t pos;
    uint32_t sampleRate;
    uint16_t bank; // 0x000 or 0x100
} DroSource;

// DRO commands:
// 00 NN     delay (NN+1) ms
// 01 LL HH  delay (LL|HH<<8)+1 ms
// 02/03     select bank low/high
// 04 RR VV  escaped write
// RR VV     normal write (RR>=5)
static uint64_t droStep(MusicSource* base, Opal* opl, bool* done)
{
    DroSource* s = (DroSource*)base;

    if (s->pos >= s->size)
    {
        *done = true;
        return 0;
    }

    uint8_t cmd = s->data[s->pos++];
    switch (cmd)
    {
        case 0x00:
        {
            if (s->pos >= s->size)
            {
                *done = true;
                return 0;
            }
            return framesForMs(s->sampleRate, (uint32_t)s->data[s->pos++] + 1);
        }
        case 0x01:
        {
            if (s->pos + 1 >= s->size)
            {
                *done = true;
                return 0;
            }
            uint32_t ms = (uint32_t)readU16Le(s->data + s->pos) + 1;
            s->pos += 2;
            return framesForMs(s->sampleRate, ms);
        }
        case 0x02:
        {
            s->bank = 0x000;
            return 0;
        }
        case 0x03:
        {
            s->bank = 0x100;
            return 0;
        }
        case 0x04:
        {
            if (s->pos + 1 >= s->size)
            {
                *done = true;
                return 0;
            }
            Opal_PortBuffered(opl, (uint16_t)(s->bank + s->data[s->pos]), s->data[s->pos + 1]);
            s->pos += 2;
            return 0;
        }
        default:
        {
            if (s->pos >= s->size)
            {
                *done = true;
                return 0;
            }
            Opal_PortBuffered(opl, (uint16_t)(s->bank + cmd), s->data[s->pos++]);
            return 0;
        }
    }
}

static void droFree(MusicSource* base)
{
    free(base);
}

static MusicSource* droLoad(const char* path, const uint8_t* data, size_t size, uint32_t sampleRate, Opal* opl)
{
    (void)path;
    (void)opl; // the command stream programs the chip itself

    // Header "DBRAWOPL" + u16 ver0 + u32 len + u32 bytes + type + pad. Only v1 handled.
    if (size < DRO_HEADER_SIZE || memcmp(data, "DBRAWOPL", 8) != 0 || readU16Le(data + 8) != 0)
    {
        return NULL;
    }

    uint32_t lengthBytes = readU32Le(data + 16);
    size_t dataSize = size - DRO_HEADER_SIZE;
    if (lengthBytes > 0 && lengthBytes <= dataSize)
    {
        dataSize = lengthBytes;
    }

    DroSource* s = (DroSource*)calloc(1, sizeof *s);
    if (s == NULL)
    {
        return NULL;
    }

    s->base.step = droStep;
    s->base.free = droFree;
    s->data = data + DRO_HEADER_SIZE;
    s->size = dataSize;
    s->sampleRate = sampleRate;
    return &s->base;
}

const MusicFormat droFormat = {"DRO", droLoad};
