/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

#include "format_hsc.h"

#include <stdlib.h>
#include <string.h>

#define HSC_CHANNELS 9
#define HSC_INSTR_COUNT 128
#define HSC_INSTR_SIZE 12
#define HSC_INSTR_BYTES (HSC_INSTR_COUNT * HSC_INSTR_SIZE)
#define HSC_ORDER_LEN 51
#define HSC_HEADER_BYTES (HSC_INSTR_BYTES + HSC_ORDER_LEN)
#define HSC_ROWS 64
#define HSC_PATTERN_BYTES (HSC_ROWS * HSC_CHANNELS * 2)
#define HSC_MAX_PATTERNS 50
#define HSC_MAX_FILE_BYTES (HSC_HEADER_BYTES + HSC_MAX_PATTERNS * HSC_PATTERN_BYTES + 1)
#define HSC_KEY_ON 0x2000
#define HSC_NOTE_OFF 0x7f
#define HSC_SET_INS 0x80
#define HSC_END 0xff
#define HSC_JUMP 0x80
#define HSC_JUMP_LIMIT 0x31
#define HSC_LEVEL_MASK 63

static const uint8_t opTable[HSC_CHANNELS] = {0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12};
static const uint16_t noteFnum[12] = {363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647, 686};

typedef struct
{
    uint8_t inst;
    uint16_t freq;
    bool dirty;
} HscChannel;

typedef struct
{
    MusicSource base;
    uint32_t sampleRate;
    uint32_t frac;
    const uint8_t* data;
    size_t dataSize;
    uint8_t instr[HSC_INSTR_COUNT][HSC_INSTR_SIZE];
    HscChannel channel[HSC_CHANNELS];
    uint8_t row;
    uint8_t orderPos;
    bool patternBreak;
    bool songEnd;
    uint32_t speed;
    uint32_t delay;
} HscSource;

static uint16_t hscNoteWord(uint8_t n)
{
    return (uint16_t)(noteFnum[n % 12] + ((uint16_t)(n / 12) << 10) + HSC_KEY_ON);
}

static uint8_t hscOrderByte(const HscSource* s, uint8_t slot)
{
    size_t idx = HSC_INSTR_BYTES + (size_t)slot;
    if (idx >= s->dataSize)
    {
        return HSC_END;
    }
    return s->data[idx];
}

static const uint8_t* hscCell(const HscSource* s, uint8_t pattern, uint8_t row, uint8_t chan)
{
    size_t off = HSC_HEADER_BYTES + (size_t)pattern * HSC_PATTERN_BYTES + ((size_t)row * HSC_CHANNELS + chan) * 2;
    if (off + 1 >= s->dataSize)
    {
        return NULL;
    }
    return s->data + off;
}

static bool hscAdditive(const uint8_t* ins)
{
    return (ins[8] & 1) != 0;
}

static void hscWriteFreq(HscSource* s, Opal* opl, uint8_t chan)
{
    const uint8_t* ins = s->instr[s->channel[chan].inst & (HSC_INSTR_COUNT - 1)];
    if (ins[11] == 0 && !s->channel[chan].dirty)
    {
        return;
    }
    s->channel[chan].dirty = false;
    uint8_t lo = (uint8_t)s->channel[chan].freq;
    uint8_t b0 = (uint8_t)(s->channel[chan].freq >> 8);
    opalWriteRegBuffered(opl, (uint16_t)(0xa0 + chan), (uint8_t)(lo + ins[11]));
    opalWriteRegBuffered(opl, (uint16_t)(0xb0 + chan), b0);
}

static void hscSlideFreq(HscSource* s, uint8_t chan, uint8_t delta, bool up)
{
    uint8_t lo = (uint8_t)s->channel[chan].freq;
    if (up)
    {
        lo = (uint8_t)(lo + delta);
    }
    else
    {
        lo = (uint8_t)(lo - delta);
    }
    s->channel[chan].freq = (uint16_t)((s->channel[chan].freq & 0xff00) | lo);
    s->channel[chan].dirty = true;
}

static void hscSetVolume(HscSource* s, Opal* opl, uint8_t chan, uint8_t volc, uint8_t volm)
{
    const uint8_t* ins = s->instr[s->channel[chan].inst & (HSC_INSTR_COUNT - 1)];
    uint8_t op = opTable[chan];
    opalWriteRegBuffered(opl, (uint16_t)(0x43 + op), (uint8_t)(volc | (ins[2] & ~HSC_LEVEL_MASK)));
    if (hscAdditive(ins))
    {
        opalWriteRegBuffered(opl, (uint16_t)(0x40 + op), (uint8_t)(volm | (ins[3] & ~HSC_LEVEL_MASK)));
    }
    else
    {
        opalWriteRegBuffered(opl, (uint16_t)(0x40 + op), ins[3]);
    }
}

static void hscSetInstr(HscSource* s, Opal* opl, uint8_t chan, uint8_t index)
{
    if (index >= HSC_INSTR_COUNT)
    {
        return;
    }
    if (s->channel[chan].inst == index)
    {
        return;
    }
    const uint8_t* ins = s->instr[index];
    uint8_t op = opTable[chan];
    s->channel[chan].inst = index;
    opalWriteRegBuffered(opl, (uint16_t)(0xb0 + chan), 0);
    opalWriteRegBuffered(opl, (uint16_t)(0xc0 + chan), ins[8]);
    opalWriteRegBuffered(opl, (uint16_t)(0x23 + op), ins[0]);
    opalWriteRegBuffered(opl, (uint16_t)(0x20 + op), ins[1]);
    opalWriteRegBuffered(opl, (uint16_t)(0x63 + op), ins[4]);
    opalWriteRegBuffered(opl, (uint16_t)(0x60 + op), ins[5]);
    opalWriteRegBuffered(opl, (uint16_t)(0x83 + op), ins[6]);
    opalWriteRegBuffered(opl, (uint16_t)(0x80 + op), ins[7]);
    opalWriteRegBuffered(opl, (uint16_t)(0xe3 + op), ins[9]);
    opalWriteRegBuffered(opl, (uint16_t)(0xe0 + op), ins[10]);
    hscSetVolume(s, opl, chan, (uint8_t)(ins[2] & HSC_LEVEL_MASK), (uint8_t)(ins[3] & HSC_LEVEL_MASK));
}

static void hscSetSpeed(HscSource* s, uint8_t nibble)
{
    s->speed = (uint32_t)nibble + 1;
    s->delay = s->speed;
}

static void hscPlayNote(HscSource* s, Opal* opl, uint8_t chan, uint8_t note)
{
    if (note == HSC_NOTE_OFF)
    {
        s->channel[chan].freq = (uint16_t)(s->channel[chan].freq & ~HSC_KEY_ON);
        s->channel[chan].dirty = true;
        return;
    }
    uint8_t n = (uint8_t)(note - 1);
    opalWriteRegBuffered(opl, (uint16_t)(0xb0 + chan), 0);
    s->channel[chan].freq = hscNoteWord(n);
    s->channel[chan].dirty = true;
}

static void hscResolveJump(HscSource* s)
{
    uint8_t v = hscOrderByte(s, s->orderPos);
    if ((v & HSC_JUMP) == 0)
    {
        return;
    }
    uint8_t dest = (v == HSC_END) ? 0 : (uint8_t)(v - HSC_JUMP);
    if (dest >= HSC_JUMP_LIMIT)
    {
        dest = 0;
    }
    s->orderPos = dest;
    s->songEnd = true;
}

static void hscAdvanceOrder(HscSource* s)
{
    uint8_t prev = s->orderPos;
    s->orderPos++;
    hscResolveJump(s);
    if (prev < HSC_ORDER_LEN && s->orderPos >= HSC_ORDER_LEN)
    {
        s->songEnd = true;
    }
}

static void hscPlayRow(HscSource* s, Opal* opl, uint8_t pattern)
{
    for (uint8_t chan = 0; chan < HSC_CHANNELS; chan++)
    {
        const uint8_t* cell = hscCell(s, pattern, s->row, chan);
        if (cell == NULL)
        {
            continue;
        }
        uint8_t note = cell[0];
        uint8_t effect = cell[1];

        if (note == HSC_SET_INS)
        {
            hscSetInstr(s, opl, chan, (uint8_t)(effect & (HSC_INSTR_COUNT - 1)));
            continue;
        }

        if (note != 0)
        {
            hscPlayNote(s, opl, chan, note);
        }

        if (effect == 0)
        {
            continue;
        }
        if (effect == 0x01)
        {
            s->patternBreak = true;
            continue;
        }

        uint8_t effOp = (uint8_t)(effect & 0x0f);
        const uint8_t* ins = s->instr[s->channel[chan].inst & (HSC_INSTR_COUNT - 1)];
        switch (effect & 0xf0)
        {
            case 0x10:
            case 0x20:
            {
                hscSlideFreq(s, chan, (uint8_t)(effOp + 1), (effect & 0x10) != 0);
                break;
            }
            case 0xa0:
            {
                opalWriteRegBuffered(opl, (uint16_t)(0x43 + opTable[chan]), (uint8_t)(effOp << 2));
                break;
            }
            case 0xb0:
            {
                opalWriteRegBuffered(opl, (uint16_t)(0x40 + opTable[chan]), (uint8_t)(effOp << 2));
                break;
            }
            case 0xc0:
            {
                uint8_t db = (uint8_t)(effOp << 2);
                opalWriteRegBuffered(opl, (uint16_t)(0x43 + opTable[chan]), db);
                if (hscAdditive(ins))
                {
                    opalWriteRegBuffered(opl, (uint16_t)(0x40 + opTable[chan]), db);
                }
                break;
            }
            default:
            {
                hscSetSpeed(s, effOp);
                break;
            }
        }
    }

    for (uint8_t chan = 0; chan < HSC_CHANNELS; chan++)
    {
        hscWriteFreq(s, opl, chan);
    }
}

static bool hscUpdate(HscSource* s, Opal* opl)
{
    s->delay--;
    if (s->delay != 0)
    {
        return !s->songEnd;
    }

    uint8_t pattern = hscOrderByte(s, s->orderPos);
    hscPlayRow(s, opl, pattern);

    s->delay = s->speed;
    if (s->patternBreak)
    {
        s->row = 0;
        s->patternBreak = false;
        hscAdvanceOrder(s);
    }
    else
    {
        s->row++;
        if (s->row == HSC_ROWS)
        {
            s->row = 0;
            hscAdvanceOrder(s);
        }
    }
    return !s->songEnd;
}

static uint64_t hscStep(MusicSource* base, Opal* opl, bool* done)
{
    HscSource* s = (HscSource*)base;
    if (!hscUpdate(s, opl))
    {
        *done = true;
        s->songEnd = false;
    }

    // 18.2 Hz PIT tick: sampleRate * 5 / 91, remainder in frac
    uint64_t num = (uint64_t)s->sampleRate * 5 + s->frac;
    s->frac = (uint32_t)(num % 91);
    return num / 91;
}

static void hscFree(MusicSource* base)
{
    free(base);
}

static bool endsWithHsc(const char* path)
{
    size_t n = strlen(path);
    if (n < 4)
    {
        return false;
    }
    const char* e = path + n - 4;
    return e[0] == '.' && (e[1] == 'h' || e[1] == 'H') && (e[2] == 's' || e[2] == 'S') && (e[3] == 'c' || e[3] == 'C');
}

static MusicSource* hscLoad(const char* path, const uint8_t* data, size_t size, uint32_t sampleRate, Opal* opl)
{
    if (!endsWithHsc(path))
    {
        return NULL;
    }
    if (size < HSC_HEADER_BYTES + HSC_PATTERN_BYTES || size > HSC_MAX_FILE_BYTES)
    {
        return NULL;
    }

    HscSource* s = (HscSource*)calloc(1, sizeof *s);
    if (s == NULL)
    {
        return NULL;
    }

    s->base.step = hscStep;
    s->base.free = hscFree;
    s->base.loop = true;
    s->sampleRate = sampleRate != 0 ? sampleRate : (uint32_t)OPAL_OPL3_SAMPLE_RATE;
    s->data = data;
    s->dataSize = size;

    memcpy(s->instr, data, HSC_INSTR_BYTES);
    for (int i = 0; i < HSC_INSTR_COUNT; i++)
    {
        s->instr[i][2] ^= (uint8_t)((s->instr[i][2] & 0x40) << 1);
        if (hscAdditive(s->instr[i]))
        {
            s->instr[i][3] ^= (uint8_t)((s->instr[i][3] & 0x40) << 1);
        }
        s->instr[i][11] >>= 4;
    }

    s->speed = 2;
    s->delay = 1;
    for (uint8_t i = 0; i < HSC_CHANNELS; i++)
    {
        s->channel[i].inst = 0xFF;
    }

    opalWriteRegBuffered(opl, 0x01, 0x20);
    opalWriteRegBuffered(opl, 0x08, 0x40);
    opalWriteRegBuffered(opl, 0xBD, 0x00);
    for (uint8_t i = 0; i < HSC_CHANNELS; i++)
    {
        hscSetInstr(s, opl, i, i);
    }

    opalFlushWriteBuf(opl);

    return &s->base;
}

const MusicFormat hscFormat = {"HSC", hscLoad};
