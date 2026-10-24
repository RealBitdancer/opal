/*
    Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
    SPDX-License-Identifier: MIT

    Opal OPL3 emulator (public API).

    Emulates the YMF262 (OPL3), including the OPL2 compatibility mode that runs YM3812
    software. 2/4 op FM, 8 waveforms, LFOs, rhythm, timers, status via opalReadStatus.
    Register 08 bit 7 is unused on the YMF262, as on the real chip. While NEW
    (register 105h bit 0) is clear, bank 1 writes other than 105h land in bank 0, E0h writes
    keep only waveforms 0-3, and C0h writes turn on both CHA and CHB. Changing NEW never
    changes what was already written: all 18 channels always play, and four-operator pairs
    follow register 104h alone.

    The Opal struct holds no pointers: all internal references are indices, so an instance is
    a plain value. Copy, memcpy, or relocate it freely. A copy is a complete save state.
*/

#ifndef OPAL_OPAL_H_INCLUDED
#define OPAL_OPAL_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct OpalChannel OpalChannel;
typedef struct OpalOperator OpalOperator;
typedef struct OpalQueuedWrite OpalQueuedWrite;
typedef struct Opal Opal;

// clang-format off
#define OPAL_VERSION_MAJOR 3
#define OPAL_VERSION_MINOR 0
#define OPAL_VERSION_PATCH 0

#define OPAL_VERSION ((OPAL_VERSION_MAJOR) * 10000 + (OPAL_VERSION_MINOR) * 100 + (OPAL_VERSION_PATCH))

/* Various constants */
#define OPAL_OPL3_SAMPLE_RATE 49716
#define OPAL_CHANNEL_COUNT    18
#define OPAL_OPERATOR_COUNT   36

#define OPAL_WRITEBUF_SIZE  4096
#define OPAL_WRITEBUF_DELAY 2

/* Sentinel values for the index fields below */
#define OPAL_OP_NONE        0xFF    /* no operator: the source reads as silence */
#define OPAL_MOD_OWN_FB     0xFE    /* modSource: the operator's own feedback value */
#define OPAL_CH_NONE        0xFF    /* no paired channel */

/* OpalOperator::envelopeStage values */
#define OPAL_ENVELOPE_STAGE_OFF     (-1) /* never written by the core */
#define OPAL_ENVELOPE_STAGE_ATTACK  0
#define OPAL_ENVELOPE_STAGE_DECAY   1
#define OPAL_ENVELOPE_STAGE_SUSTAIN 2
#define OPAL_ENVELOPE_STAGE_RELEASE 3

/* OpalChannel::chanType values */
#define OPAL_CHANNEL_TYPE_2OP   0   /* two-operator melodic channel */
#define OPAL_CHANNEL_TYPE_4OP   1   /* primary half of a four-operator pair */
#define OPAL_CHANNEL_TYPE_4OP2  2   /* secondary half of a four-operator pair */
#define OPAL_CHANNEL_TYPE_DRUM  3   /* rhythm-mode percussion channel */

/*
    A single FM operator (maps to one OPL3 slot).

    Slot layout: melodic channel ch (0-8 = bank 0, 9-17 = bank 1) uses op[slot] as its modulator
    and op[slot + 3] as its carrier, where

        slot = { 0,1,2, 6,7,8, 12,13,14, 18,19,20, 24,25,26, 30,31,32 }[ch]

    In 4-op mode the paired channel contributes op[slot + 6] and op[slot + 9]. The same operators
    are reachable through OpalChannel::opSlot[]. All cross references are indices into Opal::op
    and Opal::chan, so a copied instance is fully self-contained.

    Fields useful for visualizers:
        envelopeStage   ATTACK, DECAY, SUSTAIN, or RELEASE. OPAL_ENVELOPE_STAGE_OFF is defined
                        for visualizers. The core never writes it: idle operators sit in RELEASE
                        with egOut >= 511
        egOut           current total attenuation (envelope + output level + KSL + tremolo) in
                        0.1875 dB steps where 0 is loudest and >= 511 is silence
        key             nonzero while keyed on (bit 0 = normal key on, bit 1 = rhythm key on)
        modSource       phase modulation input: an index into Opal::op whose out is read,
                        OPAL_MOD_OWN_FB for the operator's own feedback, OPAL_OP_NONE for none
        out             the operator's latest output sample
*/
struct OpalOperator
{
    int             envelopeStage;
    uint16_t        egOut;
    uint8_t         key;
    uint8_t         modSource;
    int16_t         out;
    int16_t         prevOut;
    uint32_t        phase;
    uint16_t        envelope;
    uint8_t         tremolo;
    uint8_t         vibrato;
    uint8_t         sustainHold;
    uint8_t         keyScaleRate;
    uint8_t         multiple;
    uint8_t         keyScaleLevel;
    uint8_t         totalLevel;
    uint8_t         attackRate;
    uint8_t         decayRate;
    uint8_t         sustainLevel;
    uint8_t         releaseRate;
    uint8_t         waveform;
};

/*
    A single channel, which can contain two or more operators.

    opSlot[] holds the indices of the channel's operators in Opal::op (OPAL_OP_NONE when absent).
    pairIndex is the physical 4-op partner (0+3, 1+4, 2+5 and the same on bank 1). Init pre-wires
    those pairs. Channels that cannot pair (6-8, 15-17) use OPAL_CH_NONE. Whether 4-op is active
    is chanType, not pairIndex. outSource[] lists up to four operator indices whose out values
    sum into the channel output, describing the active FM algorithm routing.
*/
struct OpalChannel
{
    uint8_t         opSlot[4];
    uint8_t         pairIndex;
    uint8_t         outSource[4];
    int             chanType;
    uint16_t        leftPan;
    uint16_t        rightPan;
    uint16_t        fNumber;
    uint8_t         block;
    uint8_t         latchedKeyScaleNumber;
    uint8_t         feedback;
    uint8_t         connection;
    uint8_t         outputEnable;
    uint8_t         algorithm;
};

struct OpalQueuedWrite
{
    uint64_t        due;
    uint16_t        regNum;
    uint8_t         value;
};


/*==================================================================================================
            Opal instance.

            A plain value with no internal pointers: every cross reference is an index. Copying
            or relocating an instance (assignment, memcpy, reallocating containers) yields a
            fully working chip, so a copy doubles as a save state or a snapshot for a
            visualizer.
 ==================================================================================================*/
struct Opal
{
    int32_t             sampleRate;
    int32_t             sampleAccum;
    int16_t             lastOutput[2], currOutput[2];
    OpalChannel         chan[OPAL_CHANNEL_COUNT];
    OpalOperator        op[OPAL_OPERATOR_COUNT];
    uint8_t             timer[2];
    uint8_t             timerControl;
    uint16_t            timerCount[2];
    uint8_t             status;
    uint8_t             newMode;
    uint8_t             pairEnable;
    uint8_t             rhythmMode;
    uint8_t             noteSelect;
    uint8_t             tremoloDepth;
    uint8_t             vibratoDepth;
    uint8_t             waveSelectEnable;
    uint8_t             compositeSine;
    uint8_t             tremoloPosition;
    uint8_t             tremoloLevel;
    uint16_t            sampleCounter;
    uint16_t            hiHatPhase;
    uint16_t            cymbalPhase;
    uint32_t            noise;
    int32_t             pendingRight;
    uint64_t            chipSamples;
    uint64_t            lastDue;
    uint16_t            queueHead;
    uint16_t            queueCount;
    OpalQueuedWrite     queue[OPAL_WRITEBUF_SIZE];
};
// clang-format on

/*==================================================================================================
            Public API.
 ==================================================================================================*/
/*!
 * \brief Initialize Opal with a given output sample rate, resetting all chip state
 * \param self Pointer to the Opal instance
 * \param sampleRate Desired output sample rate in Hz (0 or less selects 49716, values above
 *        INT32_MAX - 49716 are clamped to it)
 */
void opalInit(Opal* self, int sampleRate);

/*!
 * \brief Change the output sample rate of an existing initialized instance
 * \param self Pointer to the Opal instance
 * \param sampleRate Desired output sample rate in Hz (0 or less selects 49716, values above
 *        INT32_MAX - 49716 are clamped to it)
 */
void opalSetSampleRate(Opal* self, int sampleRate);

/*!
 * \brief Write a register immediately (same sample as the call)
 * \param self Pointer to the Opal instance
 * \param regNum OPL3 Register address
 * \param val Value to write
 */
void opalWriteReg(Opal* self, uint16_t regNum, uint8_t val);

/*!
 * \brief Queue a register write. Queued writes land at least OPAL_WRITEBUF_DELAY chip samples
 *        apart, so a key-off followed by a key-on still restarts the envelope. The spacing is an
 *        emulator convention, much slower than the chip's bus, so long bursts fall behind.
 * \param self Pointer to the Opal instance
 * \param regNum OPL3 Register address
 * \param val Value to write
 */
void opalWriteRegBuffered(Opal* self, uint16_t regNum, uint8_t val);

/*!
 * \brief Apply any queued register writes immediately
 * \param self Pointer to the Opal instance
 */
void opalFlushWriteBuf(Opal* self);

/*!
 * \brief Set panning level per channel
 * \param self Pointer to the Opal instance
 * \param channel Channel index 0 to 17, or 0 to 8 with bit 8 set for the second bank
 * \param pan Panning level (0 left, 64 middle, 127 right, higher values clamp to 127)
 */
void opalPan(Opal* self, uint16_t channel, uint8_t pan);

/*!
 * \brief Generate one stereo 16-bit PCM sample (writes two integers)
 * \param self Pointer to the Opal instance
 * \param left Sample for the left channel
 * \param right Sample for the right channel
 */
void opalSample(Opal* self, int16_t* left, int16_t* right);

/*!
 * \brief Read the chip status register (bit 7 = IRQ, bit 6 = Timer 1 overflow, bit 5 = Timer 2
 *        overflow). A flag stays set until a reset write (bit 7 of register 4) or a write that
 *        sets its timer's mask bit. IRQ stays set while any flag is. A masked timer sets
 *        neither its flag nor IRQ.
 * \param self Pointer to the Opal instance
 * \return The status byte, as a real OPL would return on a status-port read
 */
uint8_t opalReadStatus(Opal* self);

#ifdef __cplusplus
}
#endif

#endif /* OPAL_OPAL_H_INCLUDED */
