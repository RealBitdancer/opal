/*
    Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
    SPDX-License-Identifier: MIT

    Opal OPL3 emulator.

    Emulation core by Shayde/Reality (Reality Adlib Tracker 2). Public domain. See PUBLIC_DOMAIN_PROOF.txt.
    C11 port from OpenMPT soundlib/opal.h. C API, resampling, pan, rhythm, timers, and accuracy
    work by Bitdancer. See LICENSE and THIRD_PARTY_LICENSES.md.

    Stereo output matches YMF262 reference emulator behavior.
*/

#include "opal/opal.h"

#include <stdint.h>
#include <string.h>

enum
{
    OpalCh2Op = 0,
    OpalCh4Op = 1,
    OpalCh4Op2 = 2,
    OpalChDrum = 3
};

enum
{
    OpalKeyNorm = 0x01,
    OpalKeyDrum = 0x02
};

enum
{
    EnvAtt = 0,
    EnvDec = 1,
    EnvSus = 2,
    EnvRel = 3,
    EnvOff = -1
};

// clang-format off
static const int8_t ad_slot[0x20] =
{
    0,  1,  2,  3,  4,  5,  -1, -1, 6,  7,  8,  9,  10, 11, -1, -1,
    12, 13, 14, 15, 16, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static const uint8_t ch_slot[18] =
{
    0, 1, 2, 6, 7, 8, 12, 13, 14, 18, 19, 20, 24, 25, 26, 30, 31, 32,
};

static const uint16_t MulTimes2[16] =
{
    1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30,
};

static const uint8_t kslshift[4] =
{
    8, 1, 2, 0
};

static const uint8_t kslrom[16] =
{
    0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64
};

static const uint8_t eg_incstep[4][4] = 
{
    {0, 0, 0, 0},
    {1, 0, 0, 0},
    {1, 0, 1, 0},
    {1, 1, 1, 0},
};

/* Decapped OPL2/OPL3 chip ROM. Hardware constants; see THIRD_PARTY_LICENSES.md. */
static const uint16_t ExpTable[0x100] =
{
    0x7FA, 0x7F5, 0x7EF, 0x7EA, 0x7E4, 0x7DF, 0x7DA, 0x7D4,
    0x7CF, 0x7C9, 0x7C4, 0x7BF, 0x7B9, 0x7B4, 0x7AE, 0x7A9,
    0x7A4, 0x79F, 0x799, 0x794, 0x78F, 0x78A, 0x784, 0x77F,
    0x77A, 0x775, 0x770, 0x76A, 0x765, 0x760, 0x75B, 0x756,
    0x751, 0x74C, 0x747, 0x742, 0x73D, 0x738, 0x733, 0x72E,
    0x729, 0x724, 0x71F, 0x71A, 0x715, 0x710, 0x70B, 0x706,
    0x702, 0x6FD, 0x6F8, 0x6F3, 0x6EE, 0x6E9, 0x6E5, 0x6E0,
    0x6DB, 0x6D6, 0x6D2, 0x6CD, 0x6C8, 0x6C4, 0x6BF, 0x6BA,
    0x6B5, 0x6B1, 0x6AC, 0x6A8, 0x6A3, 0x69E, 0x69A, 0x695,
    0x691, 0x68C, 0x688, 0x683, 0x67F, 0x67A, 0x676, 0x671,
    0x66D, 0x668, 0x664, 0x65F, 0x65B, 0x657, 0x652, 0x64E,
    0x649, 0x645, 0x641, 0x63C, 0x638, 0x634, 0x630, 0x62B,
    0x627, 0x623, 0x61E, 0x61A, 0x616, 0x612, 0x60E, 0x609,
    0x605, 0x601, 0x5FD, 0x5F9, 0x5F5, 0x5F0, 0x5EC, 0x5E8,
    0x5E4, 0x5E0, 0x5DC, 0x5D8, 0x5D4, 0x5D0, 0x5CC, 0x5C8,
    0x5C4, 0x5C0, 0x5BC, 0x5B8, 0x5B4, 0x5B0, 0x5AC, 0x5A8,
    0x5A4, 0x5A0, 0x59C, 0x599, 0x595, 0x591, 0x58D, 0x589,
    0x585, 0x581, 0x57E, 0x57A, 0x576, 0x572, 0x56F, 0x56B,
    0x567, 0x563, 0x560, 0x55C, 0x558, 0x554, 0x551, 0x54D,
    0x549, 0x546, 0x542, 0x53E, 0x53B, 0x537, 0x534, 0x530,
    0x52C, 0x529, 0x525, 0x522, 0x51E, 0x51B, 0x517, 0x514,
    0x510, 0x50C, 0x509, 0x506, 0x502, 0x4FF, 0x4FB, 0x4F8,
    0x4F4, 0x4F1, 0x4ED, 0x4EA, 0x4E7, 0x4E3, 0x4E0, 0x4DC,
    0x4D9, 0x4D6, 0x4D2, 0x4CF, 0x4CC, 0x4C8, 0x4C5, 0x4C2,
    0x4BE, 0x4BB, 0x4B8, 0x4B5, 0x4B1, 0x4AE, 0x4AB, 0x4A8,
    0x4A4, 0x4A1, 0x49E, 0x49B, 0x498, 0x494, 0x491, 0x48E,
    0x48B, 0x488, 0x485, 0x482, 0x47E, 0x47B, 0x478, 0x475,
    0x472, 0x46F, 0x46C, 0x469, 0x466, 0x463, 0x460, 0x45D,
    0x45A, 0x457, 0x454, 0x451, 0x44E, 0x44B, 0x448, 0x445,
    0x442, 0x43F, 0x43C, 0x439, 0x436, 0x433, 0x430, 0x42D,
    0x42A, 0x428, 0x425, 0x422, 0x41F, 0x41C, 0x419, 0x416,
    0x414, 0x411, 0x40E, 0x40B, 0x408, 0x406, 0x403, 0x400,
};

static const uint16_t LogSinTable[0x100] =
{
    2137, 1731, 1543, 1419, 1326, 1252, 1190, 1137, 1091, 1050, 1013,  979,  949,  920,  894,  869,
     846,  825,  804,  785,  767,  749,  732,  717,  701,  687,  672,  659,  646,  633,  621,  609,
     598,  587,  576,  566,  556,  546,  536,  527,  518,  509,  501,  492,  484,  476,  468,  461,
     453,  446,  439,  432,  425,  418,  411,  405,  399,  392,  386,  380,  375,  369,  363,  358,
     352,  347,  341,  336,  331,  326,  321,  316,  311,  307,  302,  297,  293,  289,  284,  280,
     276,  271,  267,  263,  259,  255,  251,  248,  244,  240,  236,  233,  229,  226,  222,  219,
     215,  212,  209,  205,  202,  199,  196,  193,  190,  187,  184,  181,  178,  175,  172,  169,
     167,  164,  161,  159,  156,  153,  151,  148,  146,  143,  141,  138,  136,  134,  131,  129,
     127,  125,  122,  120,  118,  116,  114,  112,  110,  108,  106,  104,  102,  100,   98,   96,
      94,   92,   91,   89,   87,   85,   83,   82,   80,   78,   77,   75,   74,   72,   70,   69,
      67,   66,   64,   63,   62,   60,   59,   57,   56,   55,   53,   52,   51,   49,   48,   47,
      46,   45,   43,   42,   41,   40,   39,   38,   37,   36,   35,   34,   33,   32,   31,   30,
      29,   28,   27,   26,   25,   24,   23,   23,   22,   21,   20,   20,   19,   18,   17,   17,
      16,   15,   15,   14,   13,   13,   12,   12,   11,   10,   10,    9,    9,    8,    8,    7,
       7,    7,    6,    6,    5,    5,    5,    4,    4,    4,    3,    3,    3,    2,    2,    2,
       2,    1,    1,    1,    1,    1,    1,    1,    0,    0,    0,    0,    0,    0,    0,    0,
};
// clang-format on

static int16_t Opal_ClipSample(int32_t sample)
{
    if (sample > 32767)
    {
        return 32767;
    }
    if (sample < -32768)
    {
        return -32768;
    }
    return (int16_t)sample;
}

static void Operator_UpdateKSL(OpalOperator* op)
{
    OpalChannel* chan = op->Chan;
    int16_t ksl = (int16_t)((kslrom[chan->Freq >> 6] << 2) - ((int16_t)(0x08 - chan->Octave) << 5));
    if (ksl < 0)
    {
        ksl = 0;
    }
    op->EgKsl = (uint8_t)ksl;
}

static uint16_t Operator_EffectiveWaveform(const OpalOperator* op)
{
    uint16_t wf = op->Waveform;
    if (!op->Master->OPL3Enabled)
    {
        wf &= 0x03;
    }
    return wf;
}

static int16_t Operator_ExpCalc(uint32_t level)
{
    if (level > 0x1FFF)
    {
        level = 0x1FFF;
    }
    return (int16_t)((ExpTable[level & 0xFF] << 1) >> (level >> 8));
}

static int16_t Operator_Wave(const OpalOperator* op, uint16_t phase, uint16_t envelope)
{
    uint16_t out = 0;
    uint16_t neg = 0;
    uint16_t waveform = Operator_EffectiveWaveform(op);

    phase &= 0x3FF;

    switch (waveform)
    {
        case 0:
        {
            if (phase & 0x200)
            {
                neg = 0xFFFF;
            }
            if (phase & 0x100)
            {
                out = LogSinTable[(phase & 0xFF) ^ 0xFF];
            }
            else
            {
                out = LogSinTable[phase & 0xFF];
            }
            break;
        }
        case 1:
        {
            if (phase & 0x200)
            {
                out = 0x1000;
            }
            else if (phase & 0x100)
            {
                out = LogSinTable[(phase & 0xFF) ^ 0xFF];
            }
            else
            {
                out = LogSinTable[phase & 0xFF];
            }
            break;
        }
        case 2:
        {
            if (phase & 0x100)
            {
                out = LogSinTable[(phase & 0xFF) ^ 0xFF];
            }
            else
            {
                out = LogSinTable[phase & 0xFF];
            }
            break;
        }
        case 3:
        {
            if (phase & 0x100)
            {
                out = 0x1000;
            }
            else
            {
                out = LogSinTable[phase & 0xFF];
            }
            break;
        }
        case 4:
        {
            if ((phase & 0x300) == 0x100)
            {
                neg = 0xFFFF;
            }
            if (phase & 0x200)
            {
                out = 0x1000;
            }
            else if (phase & 0x80)
            {
                out = LogSinTable[((phase ^ 0xFF) << 1) & 0xFF];
            }
            else
            {
                out = LogSinTable[(phase << 1) & 0xFF];
            }
            break;
        }
        case 5:
        {
            if (phase & 0x200)
            {
                out = 0x1000;
            }
            else if (phase & 0x80)
            {
                out = LogSinTable[((phase ^ 0xFF) << 1) & 0xFF];
            }
            else
            {
                out = LogSinTable[(phase << 1) & 0xFF];
            }
            break;
        }
        case 6:
        {
            if (phase & 0x200)
            {
                neg = 0xFFFF;
            }
            break;
        }
        default:
        {
            if (phase & 0x200)
            {
                neg = 0xFFFF;
                phase = (uint16_t)((phase & 0x1FF) ^ 0x1FF);
            }
            out = (uint16_t)(phase << 3);
            break;
        }
    }

    return (int16_t)(Operator_ExpCalc(out + ((uint32_t)envelope << 3)) ^ neg);
}

static void Operator_EnvelopeCalc(OpalOperator* op)
{
    Opal* chip = op->Master;
    uint8_t trem = op->TremoloEnable ? chip->TremoloLevel : 0;
    op->EgOut = (uint16_t)(op->EnvelopeLevel + op->OutputLevel + (op->EgKsl >> kslshift[op->KeyScaleReg]) + trem);

    uint8_t reg_rate = 0;
    uint8_t reset = 0;

    if (op->Key && op->EnvelopeStage == EnvRel)
    {
        reset = 1;
        reg_rate = (uint8_t)op->AttackRate;
    }
    else
    {
        switch (op->EnvelopeStage)
        {
            case EnvAtt:
            {
                reg_rate = (uint8_t)op->AttackRate;
                break;
            }
            case EnvDec:
            {
                reg_rate = (uint8_t)op->DecayRate;
                break;
            }
            case EnvSus:
            {
                if (!op->SustainMode)
                {
                    reg_rate = (uint8_t)op->ReleaseRate;
                }
                break;
            }
            case EnvRel:
            {
                reg_rate = (uint8_t)op->ReleaseRate;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    op->PhaseReset = reset;

    uint8_t ks = (uint8_t)(op->Chan->KeyScaleNumber >> ((op->KeyScaleRate ^ 1) << 1));
    uint8_t rate = (uint8_t)(ks + (reg_rate << 2));
    uint8_t rate_hi = (uint8_t)(rate >> 2);
    uint8_t rate_lo = (uint8_t)(rate & 3);
    if (rate_hi & 0x10)
    {
        rate_hi = 0x0F;
    }

    uint8_t eg_shift = (uint8_t)(rate_hi + chip->EgAdd);
    uint8_t shift = 0;
    if (reg_rate != 0)
    {
        if (rate_hi < 12)
        {
            if (chip->EgState)
            {
                switch (eg_shift)
                {
                    case 12:
                    {
                        shift = 1;
                        break;
                    }
                    case 13:
                    {
                        shift = (uint8_t)((rate_lo >> 1) & 1);
                        break;
                    }
                    case 14:
                    {
                        shift = (uint8_t)(rate_lo & 1);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            shift = (uint8_t)((rate_hi & 3) + eg_incstep[rate_lo][chip->EgTimerLo]);
            if (shift & 0x04)
            {
                shift = 0x03;
            }
            if (!shift)
            {
                shift = chip->EgState;
            }
        }
    }

    uint16_t eg_rout = op->EnvelopeLevel;
    int16_t eg_inc = 0;
    uint8_t eg_off = 0;

    if (reset && rate_hi == 0x0F)
    {
        eg_rout = 0;
    }

    if ((op->EnvelopeLevel & 0x1F8) == 0x1F8)
    {
        eg_off = 1;
    }

    if (op->EnvelopeStage != EnvAtt && !reset && eg_off)
    {
        eg_rout = 0x1FF;
    }

    switch (op->EnvelopeStage)
    {
        case EnvAtt:
        {
            if (!eg_rout)
            {
                op->EnvelopeStage = EnvDec;
            }
            else if (op->Key && shift > 0 && rate_hi != 0x0F)
            {
                eg_inc = (int16_t)(~eg_rout >> (4 - shift));
            }
            break;
        }
        case EnvDec:
        {
            if ((eg_rout >> 4) == (op->SustainLevel >> 4))
            {
                op->EnvelopeStage = EnvSus;
            }
            else if (!eg_off && !reset && shift > 0)
            {
                eg_inc = (int16_t)(1 << (shift - 1));
            }
            break;
        }
        case EnvSus:
        case EnvRel:
        {
            if (!eg_off && !reset && shift > 0)
            {
                eg_inc = (int16_t)(1 << (shift - 1));
            }
            break;
        }
        default:
        {
            break;
        }
    }

    op->EnvelopeLevel = (uint16_t)((eg_rout + eg_inc) & 0x1FF);

    if (reset)
    {
        op->EnvelopeStage = EnvAtt;
    }

    if (!op->Key)
    {
        op->EnvelopeStage = EnvRel;
    }
}

static void Operator_PhaseGenerate(OpalOperator* op)
{
    Opal* chip = op->Master;
    OpalChannel* chan = op->Chan;
    uint16_t f_num = chan->Freq;

    if (op->VibratoEnable)
    {
        int8_t range = (int8_t)((f_num >> 7) & 7);
        uint8_t vibpos = chip->VibPos;

        if (!(vibpos & 3))
        {
            range = 0;
        }
        else if (vibpos & 1)
        {
            range = (int8_t)(range >> 1);
        }
        range = (int8_t)(range >> chip->VibShift);

        if (vibpos & 4)
        {
            range = (int8_t)-range;
        }

        f_num = (uint16_t)(f_num + range);
    }

    uint32_t basefreq = ((uint32_t)f_num << chan->Octave) >> 1;
    uint16_t phase = (uint16_t)(op->Phase >> 9);

    if (op->PhaseReset)
    {
        op->Phase = 0;
    }

    op->Phase += (basefreq * op->FreqMultTimes2) >> 1;
    op->PhaseOut = phase;

    if (op->SlotIndex == 13)
    {
        chip->RmHhBit2 = (uint8_t)((phase >> 2) & 1);
        chip->RmHhBit3 = (uint8_t)((phase >> 3) & 1);
        chip->RmHhBit7 = (uint8_t)((phase >> 7) & 1);
        chip->RmHhBit8 = (uint8_t)((phase >> 8) & 1);
    }

    if (op->SlotIndex == 17 && (chip->RhythmReg & 0x20))
    {
        chip->RmTcBit3 = (uint8_t)((phase >> 3) & 1);
        chip->RmTcBit5 = (uint8_t)((phase >> 5) & 1);
    }

    if (chip->RhythmReg & 0x20)
    {
        // clang-format off
        uint8_t rm_xor = (uint8_t)((chip->RmHhBit2 ^ chip->RmHhBit7)
                                 | (chip->RmHhBit3 ^ chip->RmTcBit5)
                                 | (chip->RmTcBit3 ^ chip->RmTcBit5));
        // clang-format on
        uint32_t noise = chip->Noise;

        switch (op->SlotIndex)
        {
            case 13:
            {
                op->PhaseOut = (uint16_t)(rm_xor << 9);
                if (rm_xor ^ (noise & 1))
                {
                    op->PhaseOut |= 0xD0;
                }
                else
                {
                    op->PhaseOut |= 0x34;
                }
                break;
            }
            case 16:
            {
                op->PhaseOut = (uint16_t)((chip->RmHhBit8 << 9) | ((chip->RmHhBit8 ^ (noise & 1)) << 8));
                break;
            }
            case 17:
            {
                op->PhaseOut = (uint16_t)((rm_xor << 9) | 0x80);
                break;
            }
            default:
            {
                break;
            }
        }
    }

    uint32_t n_bit = ((chip->Noise >> 14) ^ chip->Noise) & 1u;
    chip->Noise = (chip->Noise >> 1) | (n_bit << 22);
}

static void Operator_CalcFB(OpalOperator* op)
{
    if (op->Chan->FeedbackShift)
    {
        op->FbMod = (int16_t)((op->PrevOut + op->Out) >> op->Chan->FeedbackShift);
    }
    else
    {
        op->FbMod = 0;
    }
    op->PrevOut = op->Out;
}

static void Operator_Generate(OpalOperator* op)
{
    int16_t mod = op->Mod ? *op->Mod : 0;
    uint16_t phase = (uint16_t)(op->PhaseOut + mod);
    op->Out = Operator_Wave(op, phase, op->EgOut);
}

static void Operator_Process(OpalOperator* op)
{
    Operator_CalcFB(op);
    Operator_EnvelopeCalc(op);
    Operator_PhaseGenerate(op);
    Operator_Generate(op);
}

static void Operator_KeyOn(OpalOperator* op, uint8_t type)
{
    op->Key |= type;
}

static void Operator_KeyOff(OpalOperator* op, uint8_t type)
{
    op->Key &= (uint8_t)~type;
}

static void Channel_ComputeKeyScaleNumber(OpalChannel* chan)
{
    uint16_t lsb = chan->Master->NoteSel ? (uint16_t)(chan->Freq >> 9) : (uint16_t)((chan->Freq >> 8) & 1);
    chan->KeyScaleNumber = (uint16_t)(chan->Octave << 1 | lsb);

    for (int i = 0; i < 2; i++)
    {
        if (chan->Op[i])
        {
            Operator_UpdateKSL(chan->Op[i]);
        }
    }
}

static void Channel_SetupAlg(OpalChannel* chan);

static void Channel_UpdateRhythm(Opal* self, uint8_t data)
{
    self->RhythmReg = (uint8_t)(data & 0x3F);
    self->RhythmMode = (data & 0x20) != 0;

    if (self->RhythmReg & 0x20)
    {
        OpalChannel* ch6 = &self->Chan[6];
        OpalChannel* ch7 = &self->Chan[7];
        OpalChannel* ch8 = &self->Chan[8];

        ch6->OutPtr[0] = &ch6->Op[1]->Out;
        ch6->OutPtr[1] = &ch6->Op[1]->Out;
        ch6->OutPtr[2] = &self->ZeroMod;
        ch6->OutPtr[3] = &self->ZeroMod;
        ch7->OutPtr[0] = &ch7->Op[0]->Out;
        ch7->OutPtr[1] = &ch7->Op[0]->Out;
        ch7->OutPtr[2] = &ch7->Op[1]->Out;
        ch7->OutPtr[3] = &ch7->Op[1]->Out;
        ch8->OutPtr[0] = &ch8->Op[0]->Out;
        ch8->OutPtr[1] = &ch8->Op[0]->Out;
        ch8->OutPtr[2] = &ch8->Op[1]->Out;
        ch8->OutPtr[3] = &ch8->Op[1]->Out;

        for (int ch = 6; ch < 9; ch++)
        {
            self->Chan[ch].ChanType = OpalChDrum;
            Channel_SetupAlg(&self->Chan[ch]);
        }

        if (self->RhythmReg & 0x01)
        {
            Operator_KeyOn(ch7->Op[0], OpalKeyDrum);
        }
        else
        {
            Operator_KeyOff(ch7->Op[0], OpalKeyDrum);
        }

        if (self->RhythmReg & 0x02)
        {
            Operator_KeyOn(ch8->Op[1], OpalKeyDrum);
        }
        else
        {
            Operator_KeyOff(ch8->Op[1], OpalKeyDrum);
        }

        if (self->RhythmReg & 0x04)
        {
            Operator_KeyOn(ch8->Op[0], OpalKeyDrum);
        }
        else
        {
            Operator_KeyOff(ch8->Op[0], OpalKeyDrum);
        }

        if (self->RhythmReg & 0x08)
        {
            Operator_KeyOn(ch7->Op[1], OpalKeyDrum);
        }
        else
        {
            Operator_KeyOff(ch7->Op[1], OpalKeyDrum);
        }

        if (self->RhythmReg & 0x10)
        {
            Operator_KeyOn(ch6->Op[0], OpalKeyDrum);
            Operator_KeyOn(ch6->Op[1], OpalKeyDrum);
        }
        else
        {
            Operator_KeyOff(ch6->Op[0], OpalKeyDrum);
            Operator_KeyOff(ch6->Op[1], OpalKeyDrum);
        }
    }
    else
    {
        for (int ch = 6; ch < 9; ch++)
        {
            OpalChannel* chan = &self->Chan[ch];
            chan->ChanType = OpalCh2Op;
            Channel_SetupAlg(chan);
            Operator_KeyOff(chan->Op[0], OpalKeyDrum);
            Operator_KeyOff(chan->Op[1], OpalKeyDrum);
        }
    }
}

static void Channel_WriteA0(OpalChannel* chan, uint8_t data)
{
    if (chan->Master->OPL3Enabled && chan->ChanType == OpalCh4Op2)
    {
        return;
    }

    chan->Freq = (uint16_t)((chan->Freq & 0x300) | data);
    Channel_ComputeKeyScaleNumber(chan);

    if (chan->Master->OPL3Enabled && chan->ChanType == OpalCh4Op && chan->ChannelPair)
    {
        chan->ChannelPair->Freq = chan->Freq;
        Channel_ComputeKeyScaleNumber(chan->ChannelPair);
    }
}

static void Channel_WriteB0(OpalChannel* chan, uint8_t data)
{
    if (chan->Master->OPL3Enabled && chan->ChanType == OpalCh4Op2)
    {
        return;
    }

    chan->Freq = (uint16_t)((chan->Freq & 0xFF) | ((data & 3) << 8));
    chan->Octave = (uint16_t)((data >> 2) & 7);
    Channel_ComputeKeyScaleNumber(chan);

    if (chan->Master->OPL3Enabled && chan->ChanType == OpalCh4Op && chan->ChannelPair)
    {
        chan->ChannelPair->Freq = chan->Freq;
        chan->ChannelPair->Octave = chan->Octave;
        Channel_ComputeKeyScaleNumber(chan->ChannelPair);
    }
}

static void Channel_SetupAlg(OpalChannel* chan)
{
    Opal* chip = chan->Master;

    if (chan->ChanType == OpalChDrum)
    {
        if (chan->ChanIndex == 7 || chan->ChanIndex == 8)
        {
            chan->Op[0]->Mod = &chip->ZeroMod;
            chan->Op[1]->Mod = &chip->ZeroMod;
            return;
        }

        if ((chan->ModulationType & 1) == 0)
        {
            chan->Op[0]->Mod = &chan->Op[0]->FbMod;
            chan->Op[1]->Mod = &chan->Op[0]->Out;
        }
        else
        {
            chan->Op[0]->Mod = &chan->Op[0]->FbMod;
            chan->Op[1]->Mod = &chip->ZeroMod;
        }
        return;
    }

    if (chan->Alg & 0x08)
    {
        return;
    }

    if (chan->Alg & 0x04)
    {
        OpalChannel* pair = chan->ChannelPair;
        pair->OutPtr[0] = &chip->ZeroMod;
        pair->OutPtr[1] = &chip->ZeroMod;
        pair->OutPtr[2] = &chip->ZeroMod;
        pair->OutPtr[3] = &chip->ZeroMod;

        switch (chan->Alg & 0x03)
        {
            case 0:
            {
                pair->Op[0]->Mod = &pair->Op[0]->FbMod;
                pair->Op[1]->Mod = &pair->Op[0]->Out;
                chan->Op[0]->Mod = &pair->Op[1]->Out;
                chan->Op[1]->Mod = &chan->Op[0]->Out;
                chan->OutPtr[0] = &chan->Op[1]->Out;
                chan->OutPtr[1] = &chip->ZeroMod;
                chan->OutPtr[2] = &chip->ZeroMod;
                chan->OutPtr[3] = &chip->ZeroMod;
                break;
            }
            case 1:
            {
                pair->Op[0]->Mod = &pair->Op[0]->FbMod;
                pair->Op[1]->Mod = &pair->Op[0]->Out;
                chan->Op[0]->Mod = &chip->ZeroMod;
                chan->Op[1]->Mod = &chan->Op[0]->Out;
                chan->OutPtr[0] = &pair->Op[1]->Out;
                chan->OutPtr[1] = &chan->Op[1]->Out;
                chan->OutPtr[2] = &chip->ZeroMod;
                chan->OutPtr[3] = &chip->ZeroMod;
                break;
            }
            case 2:
            {
                pair->Op[0]->Mod = &pair->Op[0]->FbMod;
                pair->Op[1]->Mod = &chip->ZeroMod;
                chan->Op[0]->Mod = &pair->Op[0]->Out;
                chan->Op[1]->Mod = &chan->Op[0]->Out;
                chan->OutPtr[0] = &chan->Op[1]->Out;
                chan->OutPtr[1] = &pair->Op[1]->Out;
                chan->OutPtr[2] = &chip->ZeroMod;
                chan->OutPtr[3] = &chip->ZeroMod;
                break;
            }
            default:
            {
                pair->Op[0]->Mod = &pair->Op[0]->FbMod;
                pair->Op[1]->Mod = &chip->ZeroMod;
                chan->Op[0]->Mod = &chip->ZeroMod;
                chan->Op[1]->Mod = &chan->Op[0]->Out;
                chan->OutPtr[0] = &pair->Op[1]->Out;
                chan->OutPtr[1] = &chan->Op[1]->Out;
                chan->OutPtr[2] = &chip->ZeroMod;
                chan->OutPtr[3] = &chip->ZeroMod;
                break;
            }
        }
        return;
    }

    switch (chan->Alg & 0x03)
    {
        case 0:
        {
            chan->Op[0]->Mod = &chan->Op[0]->FbMod;
            chan->Op[1]->Mod = &chan->Op[0]->Out;
            chan->OutPtr[0] = &chan->Op[1]->Out;
            chan->OutPtr[1] = &chip->ZeroMod;
            chan->OutPtr[2] = &chip->ZeroMod;
            chan->OutPtr[3] = &chip->ZeroMod;
            break;
        }
        case 1:
        {
            chan->Op[0]->Mod = &chan->Op[0]->FbMod;
            chan->Op[1]->Mod = &chip->ZeroMod;
            chan->OutPtr[0] = &chan->Op[0]->Out;
            chan->OutPtr[1] = &chan->Op[1]->Out;
            chan->OutPtr[2] = &chip->ZeroMod;
            chan->OutPtr[3] = &chip->ZeroMod;
            break;
        }
        case 2:
        {
            chan->Op[0]->Mod = &chan->Op[0]->FbMod;
            chan->Op[1]->Mod = &chan->Op[0]->Out;
            chan->OutPtr[0] = &chan->Op[1]->Out;
            chan->OutPtr[1] = &chan->Op[1]->Out;
            chan->OutPtr[2] = &chip->ZeroMod;
            chan->OutPtr[3] = &chip->ZeroMod;
            break;
        }
        default:
        {
            chan->Op[0]->Mod = &chan->Op[0]->FbMod;
            chan->Op[1]->Mod = &chip->ZeroMod;
            chan->OutPtr[0] = &chan->Op[0]->Out;
            chan->OutPtr[1] = &chan->Op[1]->Out;
            chan->OutPtr[2] = &chip->ZeroMod;
            chan->OutPtr[3] = &chip->ZeroMod;
            break;
        }
    }
}

static void Channel_UpdateAlg(OpalChannel* chan)
{
    if (chan->Master->OPL3Enabled)
    {
        if (chan->ChanType == OpalCh4Op)
        {
            chan->ChannelPair->Alg = (uint16_t)(0x04 | (chan->ModulationType << 1) | chan->ChannelPair->ModulationType);
            chan->Alg = (uint16_t)(0x08);
            Channel_SetupAlg(chan);
        }
        else if (chan->ChanType == OpalCh4Op2)
        {
            chan->Alg = (uint16_t)(0x04 | (chan->ChannelPair->ModulationType << 1) | chan->ModulationType);
            chan->ChannelPair->Alg = 0x08;
            Channel_SetupAlg(chan);
        }
        else
        {
            Channel_SetupAlg(chan);
        }
    }
    else
    {
        Channel_SetupAlg(chan);
    }
}

static void Channel_WriteC0(OpalChannel* chan, uint8_t data)
{
    chan->FeedbackShift = (uint16_t)(((data & 0x0E) >> 1) ? (9 - ((data & 0x0E) >> 1)) : 0);
    chan->ModulationType = (uint16_t)(data & 1);
    Channel_UpdateAlg(chan);

    if (chan->Master->OPL3Enabled)
    {
        chan->Cha = ((data >> 4) & 1) ? 0xFFFF : 0;
        chan->Chb = ((data >> 5) & 1) ? 0xFFFF : 0;
        chan->Chc = ((data >> 6) & 1) ? 0xFFFF : 0;
        chan->Chd = ((data >> 7) & 1) ? 0xFFFF : 0;
    }
    else
    {
        chan->Cha = 0xFFFF;
        chan->Chb = 0xFFFF;
        chan->Chc = 0;
        chan->Chd = 0;
    }

    /* Chc and Chd are stored for register completeness; stereo output uses Cha/Chb
       with the same one-sample right delay as the YMF262 pipeline. */
}

static void Channel_KeyOn(OpalChannel* chan)
{
    if (chan->Master->OPL3Enabled)
    {
        if (chan->ChanType == OpalCh4Op)
        {
            Operator_KeyOn(chan->Op[0], OpalKeyNorm);
            Operator_KeyOn(chan->Op[1], OpalKeyNorm);
            Operator_KeyOn(chan->ChannelPair->Op[0], OpalKeyNorm);
            Operator_KeyOn(chan->ChannelPair->Op[1], OpalKeyNorm);
        }
        else if (chan->ChanType == OpalCh2Op || chan->ChanType == OpalChDrum)
        {
            Operator_KeyOn(chan->Op[0], OpalKeyNorm);
            Operator_KeyOn(chan->Op[1], OpalKeyNorm);
        }
    }
    else
    {
        Operator_KeyOn(chan->Op[0], OpalKeyNorm);
        Operator_KeyOn(chan->Op[1], OpalKeyNorm);
    }
}

static void Channel_KeyOff(OpalChannel* chan)
{
    if (chan->Master->OPL3Enabled)
    {
        if (chan->ChanType == OpalCh4Op)
        {
            Operator_KeyOff(chan->Op[0], OpalKeyNorm);
            Operator_KeyOff(chan->Op[1], OpalKeyNorm);
            Operator_KeyOff(chan->ChannelPair->Op[0], OpalKeyNorm);
            Operator_KeyOff(chan->ChannelPair->Op[1], OpalKeyNorm);
        }
        else if (chan->ChanType == OpalCh2Op || chan->ChanType == OpalChDrum)
        {
            Operator_KeyOff(chan->Op[0], OpalKeyNorm);
            Operator_KeyOff(chan->Op[1], OpalKeyNorm);
        }
    }
    else
    {
        Operator_KeyOff(chan->Op[0], OpalKeyNorm);
        Operator_KeyOff(chan->Op[1], OpalKeyNorm);
    }
}

static void Channel_Set4Op(Opal* self, uint8_t data)
{
    for (uint8_t bit = 0; bit < 6; bit++)
    {
        uint8_t chnum = bit;
        if (bit >= 3)
        {
            chnum = (uint8_t)(bit + 9 - 3);
        }

        OpalChannel* primary = &self->Chan[chnum];
        OpalChannel* secondary = &self->Chan[chnum + 3];

        if ((data >> bit) & 1)
        {
            primary->ChanType = OpalCh4Op;
            secondary->ChanType = OpalCh4Op2;
            primary->ChannelPair = secondary;
            Channel_UpdateAlg(primary);
        }
        else
        {
            primary->ChanType = OpalCh2Op;
            secondary->ChanType = OpalCh2Op;
            primary->ChannelPair = NULL;
            Channel_UpdateAlg(primary);
            Channel_UpdateAlg(secondary);
        }
    }
}

static int32_t Channel_AccumOutput(const OpalChannel* chan)
{
    int32_t accm = 0;
    for (int i = 0; i < 4; i++)
    {
        if (chan->OutPtr[i])
        {
            accm += *chan->OutPtr[i];
        }
    }
    return accm;
}

static void Opal_TickTimer(Opal* self, int t)
{
    if (!(self->TimerControl & (1u << t)))
    {
        return;
    }
    if (++self->TimerCount[t] > 0xFF)
    {
        self->TimerCount[t] = self->Timer[t];
        if (!(self->TimerControl & (uint8_t)(0x40 >> t)))
        {
            self->Status |= (uint8_t)(0x80u >> t);
        }
    }
}

static void Opal_WriteReg(Opal* self, uint16_t reg_num, uint8_t val);

static void Opal_AdvanceLFOs(Opal* self)
{
    if ((self->ChipTimer & 0x3F) == 0x3F)
    {
        self->TremoloPos = (uint8_t)((self->TremoloPos + 1) % 210);
    }

    if (self->TremoloPos < 105)
    {
        self->TremoloLevel = (uint8_t)(self->TremoloPos >> self->TremoloShift);
    }
    else
    {
        self->TremoloLevel = (uint8_t)((210 - self->TremoloPos) >> self->TremoloShift);
    }

    if ((self->ChipTimer & 0x3FF) == 0x3FF)
    {
        self->VibPos = (uint8_t)((self->VibPos + 1) & 7);
    }

    self->ChipTimer++;

    if (self->EgState)
    {
        uint8_t shift = 0;
        while (shift < 13 && ((self->EgTimer >> shift) & 1) == 0)
        {
            shift++;
        }
        self->EgAdd = (uint8_t)(shift > 12 ? 0 : shift + 1);
        self->EgTimerLo = (uint8_t)(self->EgTimer & 3u);
    }

    if (self->EgTimerRem || self->EgState)
    {
        if (self->EgTimer == UINT64_C(0xFFFFFFFFF))
        {
            self->EgTimer = 0;
            self->EgTimerRem = 1;
        }
        else
        {
            self->EgTimer++;
            self->EgTimerRem = 0;
        }
    }

    self->EgState ^= 1;
}

static void Opal_Output(Opal* self, int16_t* left, int16_t* right)
{
    int32_t leftmix = 0;
    int32_t rightmix = 0;
    int i;

    /* YMF262 right channel is one sample behind left (mixbuff quirk). */
    *right = Opal_ClipSample(self->MixBuffRight);

    // YMF262 slot pipeline: slots 0-14, mix left (cha), slots 15-32, mix right (chb), slots 33-35.
    for (i = 0; i < 15; i++)
    {
        Operator_Process(&self->Op[i]);
    }

    for (i = 0; i < OpalNumChannels; i++)
    {
        OpalChannel* chan = &self->Chan[i];
        int32_t accm = Channel_AccumOutput(chan);
        int32_t cha = (int32_t)((int16_t)(accm & (int16_t)chan->Cha));
        leftmix += (cha * chan->LeftPan) >> 8;
    }

    for (i = 15; i < 18; i++)
    {
        Operator_Process(&self->Op[i]);
    }

    for (i = 18; i < 33; i++)
    {
        Operator_Process(&self->Op[i]);
    }

    for (i = 0; i < OpalNumChannels; i++)
    {
        OpalChannel* chan = &self->Chan[i];
        int32_t accm = Channel_AccumOutput(chan);
        int32_t chb = (int32_t)((int16_t)(accm & (int16_t)chan->Chb));
        rightmix += (chb * chan->RightPan) >> 8;
    }

    for (i = 33; i < OpalNumOperators; i++)
    {
        Operator_Process(&self->Op[i]);
    }

    *left = Opal_ClipSample(leftmix);
    self->MixBuffRight = rightmix;

    Opal_AdvanceLFOs(self);

    if ((self->ChipTimer & 3) == 0)
    {
        Opal_TickTimer(self, 0);
    }
    if ((self->ChipTimer & 15) == 0)
    {
        Opal_TickTimer(self, 1);
    }

    OpalWriteBuf* writebuf;
    while ((writebuf = &self->WriteBuf[self->WriteBufCur]), (writebuf->reg & 0x200) && writebuf->time <= self->WriteBufSampleCnt)
    {
        Opal_WriteReg(self, writebuf->reg & 0x1FF, writebuf->data);
        writebuf->reg = 0;
        self->WriteBufCur = (self->WriteBufCur + 1) % OPAL_WRITEBUF_SIZE;
    }
    self->WriteBufSampleCnt++;
}

void Opal_Sample(Opal* self, int16_t* left, int16_t* right)
{
    while (self->SampleAccum >= self->SampleRate)
    {
        self->LastOutput[0] = self->CurrOutput[0];
        self->LastOutput[1] = self->CurrOutput[1];
        Opal_Output(self, &self->CurrOutput[0], &self->CurrOutput[1]);
        self->SampleAccum -= self->SampleRate;
    }

    int32_t fract = (int32_t)(((int64_t)self->SampleAccum * 65536 + self->SampleRate / 2) / self->SampleRate);
    *left = (int16_t)(self->LastOutput[0] + ((fract * (self->CurrOutput[0] - self->LastOutput[0])) / 65536));
    *right = (int16_t)(self->LastOutput[1] + ((fract * (self->CurrOutput[1] - self->LastOutput[1])) / 65536));

    self->SampleAccum += OpalOPL3SampleRate;
}

void Opal_SetSampleRate(Opal* self, int sample_rate)
{
    if (sample_rate <= 0)
    {
        sample_rate = OpalOPL3SampleRate;
    }

    self->SampleRate = sample_rate;
    self->SampleAccum = 0;
    self->LastOutput[0] = self->LastOutput[1] = 0;
    self->CurrOutput[0] = self->CurrOutput[1] = 0;
    self->MixBuffRight = 0;
    self->WriteBufSampleCnt = 0;
    self->WriteBufCur = 0;
    self->WriteBufLast = 0;
    self->WriteBufLastTime = 0;

    for (uint32_t i = 0; i < OPAL_WRITEBUF_SIZE; i++)
    {
        self->WriteBuf[i].reg = 0;
    }
}

uint8_t Opal_Read(Opal* self)
{
    uint8_t status = self->Status;
    self->Status &= (uint8_t)~0xC0;
    return status;
}

void Opal_Init(Opal* self, int sample_rate)
{
    memset(self, 0, sizeof *self);

    for (int i = 0; i < OpalNumOperators; i++)
    {
        OpalOperator* op = &self->Op[i];
        op->Master = self;
        op->SlotIndex = (uint8_t)i;
        op->FreqMultTimes2 = 1;
        op->EnvelopeStage = EnvRel;
        op->EnvelopeLevel = 0x1FF;
        op->Mod = &self->ZeroMod;
    }

    for (int i = 0; i < OpalNumChannels; i++)
    {
        OpalChannel* chan = &self->Chan[i];
        chan->Master = self;
        chan->ChanIndex = (uint8_t)i;
        chan->ChanType = OpalCh2Op;
        chan->Cha = 0xFFFF;
        chan->Chb = 0xFFFF;
        chan->LeftPan = 256;
        chan->RightPan = 256;

        if ((i % 9) < 3)
        {
            chan->ChannelPair = &self->Chan[i + 3];
        }
        else if ((i % 9) < 6)
        {
            chan->ChannelPair = &self->Chan[i - 3];
        }

        uint8_t slot = ch_slot[i];
        chan->Op[0] = &self->Op[slot];
        chan->Op[1] = &self->Op[slot + 3];
        chan->Op[0]->Chan = chan;
        chan->Op[1]->Chan = chan;

        if (i < 3 || (i >= 9 && i < 12))
        {
            chan->Op[2] = &self->Op[slot + 6];
            chan->Op[3] = &self->Op[slot + 9];
            chan->Op[2]->Chan = chan;
            chan->Op[3]->Chan = chan;
        }

        Channel_SetupAlg(chan);
    }

    self->Noise = 1;
    self->TremoloShift = 4;
    self->VibShift = 1;

    Opal_SetSampleRate(self, sample_rate);
}

void Opal_Port(Opal* self, uint16_t reg_num, uint8_t val)
{
    Opal_WriteReg(self, reg_num, val);
}

void Opal_PortBuffered(Opal* self, uint16_t reg_num, uint8_t val)
{
    uint32_t writebuf_last = self->WriteBufLast;
    uint32_t next = (writebuf_last + 1) % OPAL_WRITEBUF_SIZE;

    if (next == self->WriteBufCur)
    {
        Opal_FlushWriteBuf(self);
        writebuf_last = self->WriteBufLast;
        next = (writebuf_last + 1) % OPAL_WRITEBUF_SIZE;
    }

    OpalWriteBuf* writebuf = &self->WriteBuf[writebuf_last];

    if (writebuf->reg & 0x200)
    {
        Opal_WriteReg(self, writebuf->reg & 0x1FF, writebuf->data);
        self->WriteBufCur = (writebuf_last + 1) % OPAL_WRITEBUF_SIZE;
        self->WriteBufSampleCnt = writebuf->time;
    }

    writebuf->reg = (uint16_t)(reg_num | 0x200);
    writebuf->data = val;

    uint64_t time1 = self->WriteBufLastTime + OPAL_WRITEBUF_DELAY;
    uint64_t time2 = self->WriteBufSampleCnt;
    if (time1 < time2)
    {
        time1 = time2;
    }

    writebuf->time = time1;
    self->WriteBufLastTime = time1;
    self->WriteBufLast = (writebuf_last + 1) % OPAL_WRITEBUF_SIZE;
}

void Opal_FlushWriteBuf(Opal* self)
{
    uint32_t idx = self->WriteBufCur;
    while (idx != self->WriteBufLast)
    {
        OpalWriteBuf* wb = &self->WriteBuf[idx];
        if (wb->reg & 0x200)
        {
            Opal_WriteReg(self, wb->reg & 0x1FF, wb->data);
            wb->reg = 0;
        }
        idx = (idx + 1) % OPAL_WRITEBUF_SIZE;
    }
    self->WriteBufCur = self->WriteBufLast;
    self->WriteBufLastTime = self->WriteBufSampleCnt;
}

static void Opal_WriteReg(Opal* self, uint16_t reg_num, uint8_t val)
{
    uint8_t high = (uint8_t)((reg_num >> 8) & 1);
    uint8_t regm = (uint8_t)(reg_num & 0xFF);

    if (regm == 0xBD && !high)
    {
        self->TremoloShift = (uint8_t)((((val >> 7) ^ 1) << 1) + 2);
        self->VibShift = (uint8_t)(((val >> 6) & 1) ^ 1);
        Channel_UpdateRhythm(self, val);
        return;
    }

    switch (regm & 0xF0)
    {
        case 0x00:
        {
            if (high)
            {
                switch (regm & 0x0F)
                {
                    case 0x04:
                    {
                        Channel_Set4Op(self, val);
                        break;
                    }
                    case 0x05:
                    {
                        self->OPL3Enabled = (val & 1) != 0;
                        break;
                    }
                }
            }
            else
            {
                switch (regm & 0x0F)
                {
                    case 0x01:
                    {
                        self->WaveformSelect = (val & 0x20) != 0;
                        break;
                    }
                    case 0x02:
                    {
                        self->Timer[0] = val;
                        break;
                    }
                    case 0x03:
                    {
                        self->Timer[1] = val;
                        break;
                    }
                    case 0x04:
                    {
                        if (val & 0x80)
                        {
                            self->Status = 0;
                        }
                        else
                        {
                            self->TimerControl = val;
                            if (val & 1)
                            {
                                self->TimerCount[0] = self->Timer[0];
                            }
                            if (val & 2)
                            {
                                self->TimerCount[1] = self->Timer[1];
                            }
                        }
                        break;
                    }
                    case 0x08:
                    {
                        self->NoteSel = (val & 0x40) != 0;
                        self->CompositeSineWave = (val & 0x80) != 0;
                        for (int i = 0; i < OpalNumChannels; i++)
                        {
                            Channel_ComputeKeyScaleNumber(&self->Chan[i]);
                        }
                        break;
                    }
                }
            }
            break;
        }
        case 0x20:
        case 0x30:
        {
            int8_t slot = ad_slot[regm & 0x1F];
            if (slot < 0)
            {
                break;
            }
            OpalOperator* op = &self->Op[18 * high + slot];
            op->TremoloEnable = (val & 0x80) != 0;
            op->VibratoEnable = (val & 0x40) != 0;
            op->SustainMode = (val & 0x20) != 0;
            op->KeyScaleRate = (val & 0x10) != 0;
            op->FreqMultTimes2 = MulTimes2[val & 15];
            break;
        }
        case 0x40:
        case 0x50:
        {
            int8_t slot = ad_slot[regm & 0x1F];
            if (slot < 0)
            {
                break;
            }
            OpalOperator* op = &self->Op[18 * high + slot];
            op->KeyScaleReg = (uint8_t)((val >> 6) & 3);
            op->OutputLevel = (uint16_t)((val & 0x3F) << 2);
            Operator_UpdateKSL(op);
            break;
        }
        case 0x60:
        case 0x70:
        {
            int8_t slot = ad_slot[regm & 0x1F];
            if (slot < 0)
            {
                break;
            }
            OpalOperator* op = &self->Op[18 * high + slot];
            op->AttackRate = (uint16_t)(val >> 4);
            op->DecayRate = (uint16_t)(val & 15);
            break;
        }
        case 0x80:
        case 0x90:
        {
            int8_t slot = ad_slot[regm & 0x1F];
            if (slot < 0)
            {
                break;
            }
            OpalOperator* op = &self->Op[18 * high + slot];
            op->SustainLevel = (uint16_t)(((val >> 4) < 15 ? (val >> 4) : 31) << 4);
            op->ReleaseRate = (uint16_t)(val & 15);
            break;
        }
        case 0xE0:
        case 0xF0:
        {
            int8_t slot = ad_slot[regm & 0x1F];
            if (slot < 0)
            {
                break;
            }
            OpalOperator* op = &self->Op[18 * high + slot];
            op->Waveform = (uint16_t)(val & 7);
            if (!self->OPL3Enabled)
            {
                op->Waveform &= 0x03;
            }
            break;
        }
        case 0xA0:
        {
            if ((regm & 0x0F) < 9)
            {
                Channel_WriteA0(&self->Chan[9 * high + (regm & 0x0F)], val);
            }
            break;
        }
        case 0xB0:
        {
            if ((regm & 0x0F) < 9)
            {
                OpalChannel* chan = &self->Chan[9 * high + (regm & 0x0F)];
                Channel_WriteB0(chan, val);
                if (val & 0x20)
                {
                    Channel_KeyOn(chan);
                }
                else
                {
                    Channel_KeyOff(chan);
                }
            }
            break;
        }
        case 0xC0:
        {
            if ((regm & 0x0F) < 9)
            {
                Channel_WriteC0(&self->Chan[9 * high + (regm & 0x0F)], val);
            }
            break;
        }
    }
}

void Opal_Pan(Opal* self, uint16_t reg_num, uint8_t pan)
{
    uint16_t chan_num = (uint16_t)((reg_num & 0xFF) + ((reg_num & 0x100) ? 9 : 0));
    if (chan_num >= OpalNumChannels)
    {
        return;
    }

    OpalChannel* chan = &self->Chan[chan_num];
    chan->LeftPan = (uint16_t)(pan <= 64 ? 256 : (256 * (127 - pan)) / 63);
    chan->RightPan = (uint16_t)(pan >= 64 ? 256 : (256 * pan) / 64);
}
