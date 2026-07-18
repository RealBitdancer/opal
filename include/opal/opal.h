/*
    Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
    SPDX-License-Identifier: MIT

    Opal OPL3 emulator (public API).

    Core by Shayde/Reality. Public domain. See PUBLIC_DOMAIN_PROOF.txt.
    C11 port, C API, resampling, and pan by Bitdancer. See LICENSE and THIRD_PARTY_LICENSES.md.

    Covers full OPL2/OPL3. Stereo output matches YMF262 reference behavior. 2/4 op FM, 8
    waveforms, LFOs, rhythm, timers, status via Opal_Read. CSW inert as on real YMF262. Bank 1
    always accessible.
*/

#ifndef OPAL_HHHH
#define OPAL_HHHH

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define OPAL_TRUE 1
#define OPAL_FALSE 0
typedef int opal_bool;

typedef struct OpalChannel_t OpalChannel;
typedef struct OpalOperator_t OpalOperator;
typedef struct Opal_t Opal;

// clang-format off
/* Various constants */
typedef enum OpalEnum_t
{
    OpalOPL3SampleRate      = 49716,
    OpalNumChannels         = 18,
    OpalNumOperators        = 36
} OpalEnum;

#define OPAL_WRITEBUF_SIZE  4096
#define OPAL_WRITEBUF_DELAY 2

typedef struct OpalWriteBuf_t
{
    uint64_t time;
    uint16_t reg;
    uint8_t  data;
} OpalWriteBuf;

/*
    A single FM operator (maps to one OPL3 slot).

    Slot layout: melodic channel ch (0-8 = bank 0, 9-17 = bank 1) uses Op[slot] as its modulator
    and Op[slot + 3] as its carrier, where

        slot = { 0,1,2, 6,7,8, 12,13,14, 18,19,20, 24,25,26, 30,31,32 }[ch]

    In 4-op mode the paired channel contributes Op[slot + 6] and Op[slot + 9]. The same operators
    are reachable in place through OpalChannel::Op[]. Prefer the indices when copying state out of
    the struct (e.g. a visualizer snapshotting from another thread): the Op/Chan cross pointers
    keep pointing into the original instance.

    Fields useful for visualizers:
        EnvelopeStage   -1 = off, 0 = attack, 1 = decay, 2 = sustain, 3 = release
        EgOut           current total attenuation (envelope + output level + KSL + tremolo) in
                        0.1875 dB steps where 0 is loudest and >= 511 is silence
        Key             nonzero while keyed on (bit 0 = normal key on, bit 1 = rhythm key on)
*/
struct OpalOperator_t
{
    Opal*           Master;
    OpalChannel*    Chan;
    uint8_t         SlotIndex;
    uint32_t        Phase;
    uint16_t        PhaseOut;
    opal_bool       PhaseReset;
    uint16_t        Waveform;
    uint16_t        FreqMultTimes2;
    int             EnvelopeStage;
    uint16_t        EnvelopeLevel;
    uint16_t        EgOut;
    uint8_t         EgKsl;
    uint16_t        OutputLevel;
    uint16_t        AttackRate;
    uint16_t        DecayRate;
    uint16_t        SustainLevel;
    uint16_t        ReleaseRate;
    uint8_t         KeyScaleReg;
    uint8_t         Key;
    int16_t         Out;
    int16_t         PrevOut;
    int16_t         FbMod;
    int16_t*        Mod;
    opal_bool       KeyScaleRate;
    opal_bool       SustainMode;
    opal_bool       TremoloEnable;
    opal_bool       VibratoEnable;
};

/* A single channel, which can contain two or more operators */
struct OpalChannel_t
{
    OpalOperator*   Op[4];

    Opal*           Master;
    uint8_t         ChanIndex;
    int             ChanType;
    uint16_t        Freq;
    uint16_t        Octave;
    uint16_t        KeyScaleNumber;
    uint16_t        FeedbackShift;
    uint16_t        ModulationType;
    uint16_t        Alg;
    OpalChannel*    ChannelPair;
    int16_t*        OutPtr[4];
    uint16_t        Cha;
    uint16_t        Chb;
    uint16_t        Chc;
    uint16_t        Chd;
    uint16_t        LeftPan;
    uint16_t        RightPan;
};


/*==================================================================================================
            Opal instance.

            Opal_Init wires internal cross pointers (Chan[].Op, Op[].Chan, Op[].Mod, ...) into the
            instance itself: initialize in place and do not copy or relocate an instance afterwards
            (no memcpy/assignment, no reallocating containers). After any move, call Opal_Init on
            the new address. This also resets all chip state.
 ==================================================================================================*/
struct Opal_t
{
    int32_t             SampleRate;
    int32_t             SampleAccum;
    int16_t             LastOutput[2], CurrOutput[2];
    int32_t             MixBuffRight;
    OpalChannel         Chan[OpalNumChannels];
    OpalOperator        Op[OpalNumOperators];
    int16_t             ZeroMod;
    uint16_t            ChipTimer;
    uint8_t             TremoloPos;
    uint8_t             TremoloLevel;
    uint8_t             TremoloShift;
    uint8_t             VibPos;
    uint8_t             VibShift;
    uint64_t            EgTimer;
    uint8_t             EgState;
    uint8_t             EgAdd;
    uint8_t             EgTimerLo;
    uint8_t             EgTimerRem;
    opal_bool           NoteSel;
    opal_bool           OPL3Enabled;
    opal_bool           RhythmMode;
    opal_bool           WaveformSelect;
    opal_bool           CompositeSineWave;
    uint32_t            Noise;
    uint8_t             RmHhBit2;
    uint8_t             RmHhBit3;
    uint8_t             RmHhBit7;
    uint8_t             RmHhBit8;
    uint8_t             RmTcBit3;
    uint8_t             RmTcBit5;
    uint8_t             RhythmReg;
    uint8_t             Timer[2];
    uint8_t             TimerControl;
    uint16_t            TimerCount[2];
    uint8_t             Status;
    OpalWriteBuf        WriteBuf[OPAL_WRITEBUF_SIZE];
    uint64_t            WriteBufSampleCnt;
    uint32_t            WriteBufCur;
    uint32_t            WriteBufLast;
    uint64_t            WriteBufLastTime;
};
// clang-format on

/*==================================================================================================
            Public API.
 ==================================================================================================*/
/*!
 * \brief Initialize Opal with a given output sample rate. Wires internal pointers to the
 *        instance's address, so the struct must stay where it is initialized (see the
 *        Opal instance notes above).
 * \param self Pointer to the Opal instance
 * \param sample_rate Desired output sample rate
 */
void Opal_Init(Opal* self, int sample_rate);

/*!
 * \brief Change the output sample rate of an existing initialized instance
 * \param self Pointer to the Opal instance
 * \param sample_rate Desired output sample rate
 */
void Opal_SetSampleRate(Opal* self, int sample_rate);

/*!
 * \brief Write a register immediately (same sample as the call)
 * \param self Pointer to the Opal instance
 * \param reg_num OPL3 Register address
 * \param val Value to write
 */
void Opal_Port(Opal* self, uint16_t reg_num, uint8_t val);

/*!
 * \brief Queue a register write, spaced OPAL_WRITEBUF_DELAY chip samples apart (chip-accurate timing)
 * \param self Pointer to the Opal instance
 * \param reg_num OPL3 Register address
 * \param val Value to write
 */
void Opal_PortBuffered(Opal* self, uint16_t reg_num, uint8_t val);

/*!
 * \brief Apply any queued register writes immediately
 * \param self Pointer to the Opal instance
 */
void Opal_FlushWriteBuf(Opal* self);

/*!
 * \brief Set panning level per channel
 * \param self Pointer to the Opal instance
 * \param reg_num Channel index 0 to 17, or 0 to 8 with bit 8 set for the second bank
 * \param pan Panning level (0 left, 64 middle, 127 right)
 */
void Opal_Pan(Opal* self, uint16_t reg_num, uint8_t pan);

/*!
 * \brief Generate one stereo 16-bit PCM sample (writes two integers)
 * \param self Pointer to the Opal instance
 * \param left Sample for the left channel
 * \param right Sample for the right channel
 */
void Opal_Sample(Opal* self, int16_t* left, int16_t* right);

/*!
 * \brief Read the chip status register (bit 7 = IRQ, bit 6 = Timer 1 overflow, bit 5 = Timer 2
 *        overflow). Flags stay set until a reset write (bit 7 of register 4), as on real
 *        hardware.
 * \param self Pointer to the Opal instance
 * \return The status byte, as a real OPL would return on a status-port read
 */
uint8_t Opal_Read(Opal* self);

#ifdef __cplusplus
}
#endif

#endif /* OPAL_HHHH */
