/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

// HSC decoder (.hsc). OPL2 tracker at fixed 18.2 Hz tick. Replay follows documented HSC format.
// Reference was AdPlug ChscPlayer. This code is independent. Uses opal in OPL2 mode.
//
// File: 128 instruments (1536 bytes), 51 byte order list, then 64x9 patterns of 2 bytes each.
// No signature. Identified by extension.

#include "format_hsc.h"

#include <stdlib.h>
#include <string.h>

#define HSC_INSTR_BYTES (128 * 12)                           // 1536
#define HSC_ORDER_BYTES 51                                   //
#define HSC_HEADER_BYTES (HSC_INSTR_BYTES + HSC_ORDER_BYTES) // 1587
#define HSC_PATTERN_BYTES (64 * 9 * 2)                       // 1152
#define HSC_MAX_PATTERNS 50

// Modulator operator register offset per OPL2 channel; F-numbers for the 12 semitones of an octave.
static const uint8_t opTable[9] = {0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12};
static const uint16_t noteTable[12] = {363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647, 686};

typedef struct
{
    uint8_t inst;
    int8_t slide;
    uint16_t freq;
} HscChannel;

typedef struct
{
    MusicSource base;
    uint32_t sampleRate;
    uint32_t frac; // fractional-frame carry for the 18.2 Hz (= 91/5) tick

    uint8_t instr[128][12];
    uint8_t song[128];       // order list (0..50 used; sized 128 to absorb goto targets)
    const uint8_t* patterns; // borrowed from the file buffer
    int numPatterns;

    HscChannel channel[9];
    uint8_t adlFreq[9]; // shadow of the OPL Bx (key/octave/freq-high) registers
    uint8_t pattpos, songpos, pattbreak, songend, mode6, bd, fadein;
    uint32_t speed, del;
} HscSource;

static void hscSetFreq(HscSource* s, Opal* opl, uint8_t chan, uint16_t freq)
{
    s->adlFreq[chan] = (uint8_t)((s->adlFreq[chan] & ~3) | (freq >> 8));
    opalWriteRegBuffered(opl, (uint16_t)(0xa0 + chan), (uint8_t)(freq & 0xff));
    opalWriteRegBuffered(opl, (uint16_t)(0xb0 + chan), s->adlFreq[chan]);
}

static void hscSetVolume(HscSource* s, Opal* opl, uint8_t chan, int volc, int volm)
{
    const uint8_t* ins = s->instr[s->channel[chan].inst];
    uint8_t op = opTable[chan];
    opalWriteRegBuffered(opl, (uint16_t)(0x43 + op), (uint8_t)(volc | (ins[2] & ~63)));
    if (ins[8] & 1)
    {
        opalWriteRegBuffered(opl, (uint16_t)(0x40 + op), (uint8_t)(volm | (ins[3] & ~63)));
    }
    else
    {
        opalWriteRegBuffered(opl, (uint16_t)(0x40 + op), ins[3]);
    }
}

static void hscSetInstr(HscSource* s, Opal* opl, uint8_t chan, uint8_t insnr)
{
    if (insnr >= 128)
    {
        return;
    }
    const uint8_t* ins = s->instr[insnr];
    uint8_t op = opTable[chan];

    s->channel[chan].inst = insnr;
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
    hscSetVolume(s, opl, chan, ins[2] & 63, ins[3] & 63);
}

// Play one pattern row (all 9 channels) of the given pattern at the current row position.
static void hscPlayRow(HscSource* s, Opal* opl, uint8_t pattnr)
{
    for (uint8_t chan = 0; chan < 9; chan++)
    {
        const uint8_t* cell = s->patterns + (size_t)pattnr * HSC_PATTERN_BYTES + (size_t)(s->pattpos * 9 + chan) * 2;
        uint8_t note = cell[0];
        uint8_t effect = cell[1];

        if (note & 128)
        {
            hscSetInstr(s, opl, chan, effect);
            continue;
        }

        uint8_t effOp = effect & 0x0f;
        uint8_t inst = s->channel[chan].inst;
        if (note)
        {
            s->channel[chan].slide = 0;
        }

        switch (effect & 0xf0)
        {
            case 0x00: // global
            {
                switch (effOp)
                {
                    case 1:
                    {
                        s->pattbreak++;
                        break; // jump to next pattern
                    }
                    case 3:
                    {
                        s->fadein = 31;
                        break; // fade in
                    }
                    case 5:
                    {
                        s->mode6 = 1;
                        break; // 6-voice (percussion) mode on
                    }
                    case 6:
                    {
                        s->mode6 = 0;
                        break; // 6-voice mode off
                    }
                }
                break;
            }
            case 0x10: // manual slide up
            case 0x20: // manual slide down
            {
                if (effect & 0x10)
                {
                    s->channel[chan].freq += effOp;
                    s->channel[chan].slide += effOp;
                }
                else
                {
                    s->channel[chan].freq -= effOp;
                    s->channel[chan].slide -= effOp;
                }
                if (!note)
                {
                    hscSetFreq(s, opl, chan, s->channel[chan].freq);
                }
                break;
            }
            case 0x50: // set percussion instrument (1 << x to OPL register 0xBD)
            {
                if (effOp <= 4)
                {
                    s->mode6 = 1;
                    s->bd = (uint8_t)(0x20 | (1u << effOp));
                }
                else if (effOp <= 7)
                {
                    s->bd = (uint8_t)(1u << effOp);
                    if (effOp == 5)
                    {
                        s->mode6 = 1;
                    }
                }
                opalWriteRegBuffered(opl, 0xbd, s->bd);
                break;
            }
            case 0x60: // set feedback
            {
                opalWriteRegBuffered(opl, (uint16_t)(0xc0 + chan), (uint8_t)((s->instr[s->channel[chan].inst][8] & 1) + (effOp << 1)));
                break;
            }
            case 0xa0: // set carrier volume
            {
                uint8_t vol = (uint8_t)(effOp << 2);
                opalWriteRegBuffered(opl, (uint16_t)(0x43 + opTable[chan]), (uint8_t)(vol | (s->instr[s->channel[chan].inst][2] & ~63)));
                break;
            }
            case 0xb0: // set modulator volume
            {
                uint8_t vol = (uint8_t)(effOp << 2);
                if (s->instr[inst][8] & 1)
                {
                    opalWriteRegBuffered(opl, (uint16_t)(0x40 + opTable[chan]), (uint8_t)(vol | (s->instr[s->channel[chan].inst][3] & ~63)));
                }
                else
                {
                    opalWriteRegBuffered(opl, (uint16_t)(0x40 + opTable[chan]), (uint8_t)(vol | (s->instr[inst][3] & ~63)));
                }
                break;
            }
            case 0xc0: // set instrument volume
            {
                uint8_t db = (uint8_t)(effOp << 2);
                opalWriteRegBuffered(opl, (uint16_t)(0x43 + opTable[chan]), (uint8_t)(db | (s->instr[s->channel[chan].inst][2] & ~63)));
                if (s->instr[inst][8] & 1)
                {
                    opalWriteRegBuffered(opl, (uint16_t)(0x40 + opTable[chan]), (uint8_t)(db | (s->instr[s->channel[chan].inst][3] & ~63)));
                }
                break;
            }
            case 0xd0: // position jump
            {
                s->pattbreak++;
                s->songpos = effOp;
                s->songend = 1;
                break;
            }
            case 0xf0: // set speed
            {
                s->speed = effOp;
                s->del = ++s->speed;
                break;
            }
        }

        if (s->fadein)
        {
            hscSetVolume(s, opl, chan, s->fadein * 2, s->fadein * 2);
        }

        if (!note)
        {
            continue;
        }
        note--;

        if ((note == 0x7f - 1) || ((note / 12) & ~7))
        {
            s->adlFreq[chan] &= ~32;
            opalWriteRegBuffered(opl, (uint16_t)(0xb0 + chan), s->adlFreq[chan]);
            continue;
        }

        uint8_t okt = (uint8_t)(((note / 12) & 7) << 2);
        uint16_t fnr = (uint16_t)(noteTable[note % 12] + s->instr[inst][11] + s->channel[chan].slide);
        s->channel[chan].freq = fnr;
        if (!s->mode6 || chan < 6)
        {
            s->adlFreq[chan] = (uint8_t)(okt | 32);
        }
        else
        {
            s->adlFreq[chan] = okt;
        }
        opalWriteRegBuffered(opl, (uint16_t)(0xb0 + chan), 0);
        hscSetFreq(s, opl, chan, fnr);

        if (s->mode6)
        {
            switch (chan)
            {
                case 6:
                {
                    opalWriteRegBuffered(opl, 0xbd, (uint8_t)(s->bd & ~16));
                    s->bd |= 48;
                    break; // bass drum
                }
                case 7:
                {
                    opalWriteRegBuffered(opl, 0xbd, (uint8_t)(s->bd & ~1));
                    s->bd |= 33;
                    break; // hi-hat
                }
                case 8:
                {
                    opalWriteRegBuffered(opl, 0xbd, (uint8_t)(s->bd & ~2));
                    s->bd |= 34;
                    break; // cymbal
                }
            }
            opalWriteRegBuffered(opl, 0xbd, s->bd);
        }
    }
}

// Process one 18.2 Hz tick. Returns false once the song has played through once.
static bool hscUpdate(HscSource* s, Opal* opl)
{
    s->del--;
    if (s->del)
    {
        return !s->songend;
    }

    if (s->fadein)
    {
        s->fadein--;
    }

    uint8_t pattnr = s->song[s->songpos];
    if (pattnr >= 0xb2)
    {
        s->songend = 1;
        s->songpos = 0;
        pattnr = s->song[s->songpos];
    }
    else if ((pattnr & 128) && (pattnr <= 0xb1))
    {
        s->songpos = s->song[s->songpos] & 127;
        s->pattpos = 0;
        pattnr = s->song[s->songpos];
        s->songend = 1;
    }

    if (pattnr < (unsigned)s->numPatterns)
    {
        hscPlayRow(s, opl, pattnr);
    }

    s->del = s->speed;
    if (s->pattbreak)
    {
        s->pattpos = 0;
        s->pattbreak = 0;
        s->songpos = (uint8_t)((s->songpos + 1) % 50);
        if (!s->songpos)
        {
            s->songend = 1;
        }
    }
    else
    {
        s->pattpos = (uint8_t)((s->pattpos + 1) & 63);
        if (!s->pattpos)
        {
            s->songpos = (uint8_t)((s->songpos + 1) % 50);
            if (!s->songpos)
            {
                s->songend = 1;
            }
        }
    }
    return !s->songend;
}

static uint64_t hscStep(MusicSource* base, Opal* opl, bool* done)
{
    HscSource* s = (HscSource*)base;
    if (!hscUpdate(s, opl))
    {
        *done = true;
    }

    // One 18.2 Hz tick = sampleRate * 5 / 91 frames, carrying the remainder so tempo does not drift.
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
    if (size < HSC_HEADER_BYTES + HSC_PATTERN_BYTES || size > 59187 + 1)
    {
        return NULL;
    }

    int numPatterns = (int)((size - HSC_HEADER_BYTES) / HSC_PATTERN_BYTES);
    if (numPatterns <= 0)
    {
        return NULL;
    }
    if (numPatterns > HSC_MAX_PATTERNS)
    {
        numPatterns = HSC_MAX_PATTERNS;
    }

    HscSource* s = (HscSource*)calloc(1, sizeof *s);
    if (s == NULL)
    {
        return NULL;
    }

    s->base.step = hscStep;
    s->base.free = hscFree;
    s->sampleRate = sampleRate;
    s->numPatterns = numPatterns;
    s->patterns = data + HSC_HEADER_BYTES;

    memcpy(s->instr, data, HSC_INSTR_BYTES);
    for (int i = 0; i < 128; i++)
    {
        s->instr[i][2] ^= (uint8_t)((s->instr[i][2] & 0x40) << 1);
        s->instr[i][3] ^= (uint8_t)((s->instr[i][3] & 0x40) << 1);
        s->instr[i][11] >>= 4; // slide value is stored in the high nibble
    }

    for (int i = 0; i < HSC_ORDER_BYTES; i++)
    {
        uint8_t v = data[HSC_INSTR_BYTES + i];
        if (((v & 0x7F) > 0x31) || ((v & 0x7F) >= numPatterns))
        {
            v = 0xFF; // out of range: ends the song here
        }
        s->song[i] = v;
    }

    // HSC reset defaults.
    s->speed = 2;
    s->del = 1;
    for (uint8_t i = 0; i < 9; i++)
    {
        s->channel[i].inst = i;
    }

    // Enable waveform select and load default instruments.
    opalWriteRegBuffered(opl, 0x01, 0x20);
    opalWriteRegBuffered(opl, 0x08, 0x80);
    opalWriteRegBuffered(opl, 0xBD, 0x00);
    for (uint8_t i = 0; i < 9; i++)
    {
        hscSetInstr(s, opl, i, i);
    }

    opalFlushWriteBuf(opl);

    return &s->base;
}

const MusicFormat hscFormat = {"HSC", hscLoad};
