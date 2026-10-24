/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

#include "format_dro.h"

#include <stdlib.h>
#include <string.h>

#define DRO_HEADER_SIZE 24
#define DRO_HEADER_SIZE_EARLY 21
#define DRO_HARDWARE_OFFSET 20
#define DRO_HARDWARE_OPL3 1
#define DRO_HARDWARE_DUAL_OPL2 2

typedef enum
{
    DRO_EVENT_END,
    DRO_EVENT_NONE,
    DRO_EVENT_DELAY,
    DRO_EVENT_WRITE
} DroEventKind;

typedef struct
{
    DroEventKind kind;
    uint32_t ms;
    uint16_t bank;
    uint8_t reg;
    uint8_t val;
} DroEvent;

typedef struct
{
    size_t pos;
    uint16_t bank;
    int lastDumpReg;
    bool inDump;
    bool escapes;
    bool twoArrays;
} DroCursor;

typedef struct
{
    MusicSource base;
    const uint8_t* data;
    size_t size;
    DroCursor cursor;
    uint32_t sampleRate;
    uint32_t frac;
    bool stereoC0;
    bool dualOpl2;
} DroSource;

static void droWrite(const DroSource* s, Opal* opl, uint16_t bank, uint8_t reg, uint8_t val)
{
    if (s->dualOpl2 && bank != 0 && reg < 0x20)
    {
        return;
    }
    if (s->stereoC0 && reg >= 0xC0 && reg <= 0xC8)
    {
        val = (uint8_t)(val | 0x30);
    }
    opalWriteRegBuffered(opl, (uint16_t)(bank + reg), val);
}

static DroEvent droEvent(DroEventKind kind, uint16_t bank, uint8_t reg, uint8_t val, uint32_t ms)
{
    DroEvent ev;
    ev.kind = kind;
    ev.ms = ms;
    ev.bank = bank;
    ev.reg = reg;
    ev.val = val;
    return ev;
}

static DroCursor droStart(bool twoArrays)
{
    DroCursor c;
    c.pos = 0;
    c.bank = 0;
    c.lastDumpReg = -1;
    c.inDump = true;
    c.escapes = false;
    c.twoArrays = twoArrays;
    return c;
}

static bool droIsEscape(const DroCursor* c, uint8_t next)
{
    return c->escapes || ((c->bank == 0) ? next <= 0x04 : next == 0x04);
}

static DroEvent droNext(DroCursor* c, const uint8_t* data, size_t size)
{
    DroEvent end = droEvent(DRO_EVENT_END, 0, 0, 0, 0);
    if (c->pos >= size)
    {
        return end;
    }

    uint8_t cmd = data[c->pos];
    if (c->inDump && c->bank == 0 && c->lastDumpReg < 0 && (cmd == 0x00 || (cmd == 0x03 && c->twoArrays)))
    {
        c->inDump = false;
        c->escapes = true;
    }
    if (c->inDump && cmd != 0x04)
    {
        if ((cmd == 0x02 || cmd == 0x03) && (int)cmd <= c->lastDumpReg)
        {
            c->pos++;
            c->bank = (cmd == 0x03) ? 0x100 : 0x000;
            c->lastDumpReg = -1;
            c->inDump = cmd == 0x03;
            return droEvent(DRO_EVENT_NONE, c->bank, 0, 0, 0);
        }
        if ((int)cmd > c->lastDumpReg)
        {
            if (c->pos + 1 >= size)
            {
                c->pos = size;
                return end;
            }
            c->lastDumpReg = cmd;
            c->pos += 2;
            return droEvent(DRO_EVENT_WRITE, c->bank, cmd, data[c->pos - 1], 0);
        }
        c->inDump = false;
    }

    c->pos++;
    switch (cmd)
    {
        case 0x00:
        {
            if (c->pos >= size)
            {
                return end;
            }
            return droEvent(DRO_EVENT_DELAY, c->bank, 0, 0, (uint32_t)data[c->pos++] + 1);
        }
        case 0x01:
        {
            if (c->pos + 1 >= size)
            {
                c->pos = size;
                return end;
            }
            uint32_t ms = (uint32_t)readU16Le(data + c->pos) + 1;
            c->pos += 2;
            return droEvent(DRO_EVENT_DELAY, c->bank, 0, 0, ms);
        }
        case 0x02:
        case 0x03:
        {
            c->bank = (cmd == 0x03) ? 0x100 : 0x000;
            return droEvent(DRO_EVENT_NONE, c->bank, 0, 0, 0);
        }
        default:
        {
            if (c->pos >= size)
            {
                return end;
            }
            uint8_t reg = cmd;
            if (cmd == 0x04 && droIsEscape(c, data[c->pos]))
            {
                if (c->pos + 1 >= size)
                {
                    c->pos = size;
                    return end;
                }
                c->escapes = c->escapes || c->bank == 0;
                c->inDump = false;
                reg = data[c->pos++];
            }
            if (c->inDump && (int)reg > c->lastDumpReg)
            {
                c->lastDumpReg = reg;
            }
            return droEvent(DRO_EVENT_WRITE, c->bank, reg, data[c->pos++], 0);
        }
    }
}

static uint64_t droStep(MusicSource* base, Opal* opl, bool* done)
{
    DroSource* s = (DroSource*)base;
    DroEvent ev = droNext(&s->cursor, s->data, s->size);
    switch (ev.kind)
    {
        case DRO_EVENT_END:
        {
            *done = true;
            return 0;
        }
        case DRO_EVENT_DELAY:
        {
            uint64_t num = (uint64_t)ev.ms * s->sampleRate + s->frac;
            s->frac = (uint32_t)(num % 1000);
            return num / 1000;
        }
        case DRO_EVENT_WRITE:
        {
            droWrite(s, opl, ev.bank, ev.reg, ev.val);
            return 0;
        }
        default:
        {
            return 0;
        }
    }
}

static bool droSnapshotEnablesOpl3(DroCursor cursor, const uint8_t* data, size_t size)
{
    for (;;)
    {
        DroEvent ev = droNext(&cursor, data, size);
        if (ev.kind == DRO_EVENT_END || ev.kind == DRO_EVENT_DELAY)
        {
            return false;
        }
        if (ev.kind == DRO_EVENT_WRITE && ev.bank != 0 && ev.reg == 0x05 && (ev.val & 0x01) != 0)
        {
            return true;
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

    // v1: low 16 bits of the version word are 0
    if (size < DRO_HEADER_SIZE_EARLY || memcmp(data, "DBRAWOPL", 8) != 0 || readU16Le(data + 8) != 0)
    {
        return NULL;
    }

    uint32_t lengthBytes = (size >= 20) ? readU32Le(data + 16) : 0;
    size_t headerSize = DRO_HEADER_SIZE; // 21-byte early header vs 24-byte padded hardware field
    if (lengthBytes > 0 && lengthBytes == size - DRO_HEADER_SIZE_EARLY)
    {
        headerSize = DRO_HEADER_SIZE_EARLY;
    }
    else if (lengthBytes > 0 && size >= DRO_HEADER_SIZE && lengthBytes == size - DRO_HEADER_SIZE)
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
    s->base.loop = false;
    s->data = data + headerSize;
    s->size = dataSize;
    s->sampleRate = sampleRate;
    uint8_t hardware = data[DRO_HARDWARE_OFFSET];
    s->cursor = droStart(hardware == DRO_HARDWARE_OPL3 || hardware == DRO_HARDWARE_DUAL_OPL2);
    s->stereoC0 = hardware != DRO_HARDWARE_OPL3;
    s->dualOpl2 = hardware == DRO_HARDWARE_DUAL_OPL2;
    if (s->dualOpl2 || droSnapshotEnablesOpl3(s->cursor, s->data, s->size))
    {
        opalWriteReg(opl, 0x105, 0x01);
    }
    return &s->base;
}

const MusicFormat droFormat = {"DRO", droLoad};
