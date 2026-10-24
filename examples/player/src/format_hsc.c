/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

#include "format_hsc.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define HSC_CHANNELS 9
#define HSC_PATCH_COUNT 128
#define HSC_RECORD_BYTES 12
#define HSC_BANK_BYTES (HSC_PATCH_COUNT * HSC_RECORD_BYTES)
#define HSC_ORDER_LEN 51
#define HSC_HEADER_BYTES (HSC_BANK_BYTES + HSC_ORDER_LEN)
#define HSC_ROWS 64
#define HSC_PATTERN_BYTES (HSC_ROWS * HSC_CHANNELS * 2)
#define HSC_MAX_PATTERNS 50
#define HSC_MAX_FILE_BYTES (HSC_HEADER_BYTES + HSC_MAX_PATTERNS * HSC_PATTERN_BYTES + 1)
#define HSC_KEY_ON 0x2000
#define HSC_NOTE_OFF 0x7f
#define HSC_SET_PATCH 0x80
#define HSC_BREAK 0x01
#define HSC_END 0xff
#define HSC_JUMP 0x80
#define HSC_JUMP_LIMIT 0x31
#define HSC_ADDITIVE 0x01
#define HSC_TL_MASK 0x3f
#define HSC_KSL_SHIFT 6
#define HSC_LEVEL_REG 0x40
#define HSC_CARRIER_OFFSET 3
#define HSC_NO_PATCH 0xff
#define HSC_START_TICKS 2
#define HSC_CMD_SLIDE_UP 0x1
#define HSC_CMD_SLIDE_DOWN 0x2
#define HSC_CMD_CARRIER_LEVEL 0xa
#define HSC_CMD_MODULATOR_LEVEL 0xb
#define HSC_CMD_VOICE_LEVEL 0xc

typedef struct
{
    uint8_t carrier;
    uint8_t modulator;
} HscOperatorPair;

typedef struct
{
    HscOperatorPair character;
    HscOperatorPair level;
    HscOperatorPair attackDecay;
    HscOperatorPair sustainRelease;
    HscOperatorPair waveform;
    uint8_t connection;
    uint8_t fineTune;
} HscPatch;

typedef struct
{
    uint8_t regBase;
    size_t member;
} HscOperatorRow;

typedef enum
{
    HSC_MODULATOR_NEVER,
    HSC_MODULATOR_ALWAYS,
    HSC_MODULATOR_IF_ADDITIVE
} HscModulatorTarget;

typedef struct
{
    bool carrier;
    HscModulatorTarget modulator;
} HscLevelCommand;

static const uint8_t operatorBase[HSC_CHANNELS] = {0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12};
static const uint16_t noteFnum[12] = {363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647, 686};
static const uint8_t kslFromFile[4] = {0, 3, 2, 1};

static const HscOperatorRow operatorRows[] = {
    {0x20, offsetof(HscPatch, character)}, {0x60, offsetof(HscPatch, attackDecay)},    {0x80, offsetof(HscPatch, sustainRelease)},
    {0xe0, offsetof(HscPatch, waveform)},  {HSC_LEVEL_REG, offsetof(HscPatch, level)},
};

static const HscLevelCommand levelCommands[] = {
    {true, HSC_MODULATOR_NEVER},
    {false, HSC_MODULATOR_ALWAYS},
    {true, HSC_MODULATOR_IF_ADDITIVE},
};

typedef struct
{
    uint8_t patchIndex;
    uint16_t freqWord;
    bool freqPending;
} HscVoice;

typedef struct
{
    MusicSource base;
    uint32_t sampleRate;
    uint32_t frac;
    const uint8_t* data;
    size_t dataSize;
    HscPatch patches[HSC_PATCH_COUNT];
    HscVoice voices[HSC_CHANNELS];
    uint8_t row;
    uint8_t orderPos;
    bool breakPending;
    bool wrapped;
    uint32_t ticksPerRow;
    uint32_t ticksRemaining;
} HscSource;

static uint16_t hscNoteWord(uint8_t n)
{
    return (uint16_t)(noteFnum[n % 12] + ((unsigned)(n / 12) << 10) + HSC_KEY_ON);
}

static uint8_t hscOrderByte(const HscSource* s, uint8_t slot)
{
    size_t idx = HSC_BANK_BYTES + (size_t)slot;
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

static uint16_t hscOperatorReg(uint8_t regBase, uint8_t chan, bool carrier)
{
    return (uint16_t)(regBase + operatorBase[chan] + (carrier ? HSC_CARRIER_OFFSET : 0));
}

static uint8_t hscNormalizeKsl(uint8_t level)
{
    return (uint8_t)((kslFromFile[level >> HSC_KSL_SHIFT] << HSC_KSL_SHIFT) | (level & HSC_TL_MASK));
}

static HscOperatorPair hscPair(uint8_t carrier, uint8_t modulator)
{
    HscOperatorPair pair;
    pair.carrier = carrier;
    pair.modulator = modulator;
    return pair;
}

static bool hscAdditive(const HscPatch* patch)
{
    return (patch->connection & HSC_ADDITIVE) != 0;
}

static void hscDecodePatch(HscPatch* patch, const uint8_t* record)
{
    patch->connection = record[8];
    uint8_t modulatorLevel = record[3];
    if (hscAdditive(patch))
    {
        modulatorLevel = hscNormalizeKsl(modulatorLevel);
    }
    patch->character = hscPair(record[0], record[1]);
    patch->level = hscPair(hscNormalizeKsl(record[2]), modulatorLevel);
    patch->attackDecay = hscPair(record[4], record[5]);
    patch->sustainRelease = hscPair(record[6], record[7]);
    patch->waveform = hscPair(record[9], record[10]);
    patch->fineTune = (uint8_t)(record[11] >> 4);
}

static const HscOperatorPair* hscPatchPair(const HscPatch* patch, size_t member)
{
    return (const HscOperatorPair*)((const uint8_t*)patch + member);
}

static const HscPatch* hscVoicePatch(const HscSource* s, uint8_t chan)
{
    return &s->patches[s->voices[chan].patchIndex & (HSC_PATCH_COUNT - 1)];
}

static void hscSelectPatch(HscSource* s, Opal* opl, uint8_t chan, uint8_t index)
{
    HscVoice* voice = &s->voices[chan];
    if (index >= HSC_PATCH_COUNT || voice->patchIndex == index)
    {
        return;
    }
    voice->patchIndex = index;
    const HscPatch* patch = &s->patches[index];

    opalWriteRegBuffered(opl, (uint16_t)(0xb0 + chan), 0);
    opalWriteRegBuffered(opl, (uint16_t)(0xc0 + chan), patch->connection);
    for (size_t i = 0; i < sizeof operatorRows / sizeof operatorRows[0]; i++)
    {
        const HscOperatorRow* row = &operatorRows[i];
        const HscOperatorPair* pair = hscPatchPair(patch, row->member);
        opalWriteRegBuffered(opl, hscOperatorReg(row->regBase, chan, true), pair->carrier);
        opalWriteRegBuffered(opl, hscOperatorReg(row->regBase, chan, false), pair->modulator);
    }
}

static void hscWriteFreq(HscSource* s, Opal* opl, uint8_t chan)
{
    HscVoice* voice = &s->voices[chan];
    const HscPatch* patch = hscVoicePatch(s, chan);
    if (patch->fineTune == 0 && !voice->freqPending)
    {
        return;
    }
    voice->freqPending = false;
    uint8_t lo = (uint8_t)voice->freqWord;
    uint8_t b0 = (uint8_t)(voice->freqWord >> 8);
    opalWriteRegBuffered(opl, (uint16_t)(0xa0 + chan), (uint8_t)(lo + patch->fineTune));
    opalWriteRegBuffered(opl, (uint16_t)(0xb0 + chan), b0);
}

static void hscSlidePitch(HscVoice* voice, int direction, uint8_t param)
{
    uint8_t lo = (uint8_t)((voice->freqWord & 0xff) + direction * ((int)param + 1));
    voice->freqWord = (uint16_t)((voice->freqWord & 0xff00) | lo);
    voice->freqPending = true;
}

static void hscPlayNote(HscSource* s, Opal* opl, uint8_t chan, uint8_t note)
{
    HscVoice* voice = &s->voices[chan];
    if (note == HSC_NOTE_OFF)
    {
        voice->freqWord = (uint16_t)(voice->freqWord & ~HSC_KEY_ON);
        voice->freqPending = true;
        return;
    }
    uint8_t n = (uint8_t)(note - 1);
    opalWriteRegBuffered(opl, (uint16_t)(0xb0 + chan), 0);
    voice->freqWord = hscNoteWord(n);
    voice->freqPending = true;
}

static void hscSetLevel(HscSource* s, Opal* opl, uint8_t chan, const HscLevelCommand* command, uint8_t level)
{
    bool toModulator = command->modulator == HSC_MODULATOR_ALWAYS || (command->modulator == HSC_MODULATOR_IF_ADDITIVE && hscAdditive(hscVoicePatch(s, chan)));
    if (command->carrier)
    {
        opalWriteRegBuffered(opl, hscOperatorReg(HSC_LEVEL_REG, chan, true), level);
    }
    if (toModulator)
    {
        opalWriteRegBuffered(opl, hscOperatorReg(HSC_LEVEL_REG, chan, false), level);
    }
}

static void hscApplyEffect(HscSource* s, Opal* opl, uint8_t chan, uint8_t effect)
{
    uint8_t command = (uint8_t)(effect >> 4);
    uint8_t param = (uint8_t)(effect & 0x0f);
    switch (command)
    {
        case HSC_CMD_SLIDE_UP:
        {
            hscSlidePitch(&s->voices[chan], 1, param);
            break;
        }
        case HSC_CMD_SLIDE_DOWN:
        {
            hscSlidePitch(&s->voices[chan], -1, param);
            break;
        }
        case HSC_CMD_CARRIER_LEVEL:
        case HSC_CMD_MODULATOR_LEVEL:
        case HSC_CMD_VOICE_LEVEL:
        {
            hscSetLevel(s, opl, chan, &levelCommands[command - HSC_CMD_CARRIER_LEVEL], (uint8_t)(param << 2));
            break;
        }
        default:
        {
            s->ticksPerRow = (uint32_t)param + 1;
            break;
        }
    }
}

static void hscAdvanceOrder(HscSource* s)
{
    uint8_t prev = s->orderPos;
    s->orderPos++;
    uint8_t v = hscOrderByte(s, s->orderPos);
    if ((v & HSC_JUMP) != 0)
    {
        uint8_t dest = (v == HSC_END) ? 0 : (uint8_t)(v - HSC_JUMP);
        s->orderPos = (dest < HSC_JUMP_LIMIT) ? dest : 0;
        s->wrapped = true;
    }
    if (prev < HSC_ORDER_LEN && s->orderPos >= HSC_ORDER_LEN)
    {
        s->wrapped = true;
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

        if (note == HSC_SET_PATCH)
        {
            hscSelectPatch(s, opl, chan, (uint8_t)(effect & (HSC_PATCH_COUNT - 1)));
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
        if (effect == HSC_BREAK)
        {
            s->breakPending = true;
            continue;
        }
        hscApplyEffect(s, opl, chan, effect);
    }

    for (uint8_t chan = 0; chan < HSC_CHANNELS; chan++)
    {
        hscWriteFreq(s, opl, chan);
    }
}

static void hscUpdate(HscSource* s, Opal* opl)
{
    s->ticksRemaining--;
    if (s->ticksRemaining != 0)
    {
        return;
    }

    uint8_t pattern = hscOrderByte(s, s->orderPos);
    hscPlayRow(s, opl, pattern);

    s->ticksRemaining = s->ticksPerRow;
    s->row++;
    if (s->breakPending || s->row >= HSC_ROWS)
    {
        s->row = 0;
        s->breakPending = false;
        hscAdvanceOrder(s);
    }
}

static uint64_t hscStep(MusicSource* base, Opal* opl, bool* done)
{
    HscSource* s = (HscSource*)base;
    if (s->wrapped && s->ticksRemaining == 1)
    {
        s->wrapped = false;
        *done = true;
        return 0;
    }
    hscUpdate(s, opl);

    // 18.2 Hz PIT tick: sampleRate * 5 / 91, remainder in frac
    uint64_t num = (uint64_t)s->sampleRate * 5 + s->frac;
    s->frac = (uint32_t)(num % 91);
    return num / 91;
}

static void hscStart(HscSource* s, Opal* opl)
{
    for (size_t k = 0; k < HSC_PATCH_COUNT; k++)
    {
        hscDecodePatch(&s->patches[k], s->data + k * HSC_RECORD_BYTES);
    }

    s->ticksPerRow = HSC_START_TICKS;
    s->ticksRemaining = 1;
    for (uint8_t chan = 0; chan < HSC_CHANNELS; chan++)
    {
        s->voices[chan].patchIndex = HSC_NO_PATCH;
    }

    opalWriteRegBuffered(opl, 0x01, 0x20);
    opalWriteRegBuffered(opl, 0x08, 0x40);
    opalWriteRegBuffered(opl, 0xbd, 0x00);
    for (uint8_t chan = 0; chan < HSC_CHANNELS; chan++)
    {
        hscSelectPatch(s, opl, chan, chan);
    }
    opalFlushWriteBuf(opl);
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
    hscStart(s, opl);

    return &s->base;
}

const MusicFormat hscFormat = {"HSC", hscLoad};
