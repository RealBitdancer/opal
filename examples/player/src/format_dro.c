/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

#include "format_dro.h"

#include <stdlib.h>
#include <string.h>

#define DRO_HEADER_SIZE 24
#define DRO_HEADER_SIZE_EARLY 21

typedef struct
{
    MusicSource base;
    const uint8_t* data;
    size_t size;
    size_t pos;
    uint32_t sampleRate;
    uint16_t bank;
} DroSource;

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
            opalWriteRegBuffered(opl, (uint16_t)(s->bank + s->data[s->pos]), s->data[s->pos + 1]);
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
            opalWriteRegBuffered(opl, (uint16_t)(s->bank + cmd), s->data[s->pos++]);
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
    (void)opl;

    // v1: low 16 bits of the version word are 0
    if (size < DRO_HEADER_SIZE_EARLY || memcmp(data, "DBRAWOPL", 8) != 0 || readU16Le(data + 8) != 0)
    {
        return NULL;
    }

    uint32_t lengthBytes = (size >= 20) ? readU32Le(data + 16) : 0;
    size_t headerSize = DRO_HEADER_SIZE; // 21-byte early header vs 24-byte padded hardware field
    if (lengthBytes > 0 && lengthBytes + DRO_HEADER_SIZE_EARLY == size)
    {
        headerSize = DRO_HEADER_SIZE_EARLY;
    }
    else if (lengthBytes > 0 && lengthBytes + DRO_HEADER_SIZE == size)
    {
        headerSize = DRO_HEADER_SIZE;
    }
    else if (size < DRO_HEADER_SIZE)
    {
        headerSize = DRO_HEADER_SIZE_EARLY;
    }
    else if ((readU32Le(data + 20) & 0xFFFFFF00u) != 0)
    {
        headerSize = DRO_HEADER_SIZE_EARLY;
    }
    size_t dataSize = size - headerSize;
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
    s->data = data + headerSize;
    s->size = dataSize;
    s->sampleRate = sampleRate;
    return &s->base;
}

const MusicFormat droFormat = {"DRO", droLoad};
