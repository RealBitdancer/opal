/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

// WebAssembly demo front-end for the opal player. Streams OPL music to Web Audio through
// miniaudio's Emscripten backend.

#include "opal/opal.h"
#include "miniaudio/miniaudio.h"

#include "player_format.h"
#include "format_dro.h"
#include "format_hsc.h"
#include "format_imf.h"

#include <emscripten/emscripten.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TAIL_MS 250

static const MusicFormat* const formats[] = {&droFormat, &hscFormat, &imfFormat};

typedef struct
{
    Opal opl;
    MusicSource* source;
    uint8_t* data;
    uint64_t wait;
    uint64_t tail;
    bool ended;
} Song;

static ma_device device;
static bool deviceReady;
static bool started;
static Song song;
static bool songReady;

static void render(void* output, ma_uint32 frameCount)
{
    int16_t* out = (int16_t*)output;
    for (ma_uint32 i = 0; i < frameCount; ++i)
    {
        int16_t left = 0;
        int16_t right = 0;

        while (song.wait == 0 && !song.ended)
        {
            bool done = false;
            song.wait += song.source->step(song.source, &song.opl, &done);
            if (done)
            {
                song.ended = true;
                Opal_FlushWriteBuf(&song.opl);
            }
        }

        if (song.wait > 0)
        {
            Opal_Sample(&song.opl, &left, &right);
            --song.wait;
        }
        else if (song.tail > 0)
        {
            Opal_Sample(&song.opl, &left, &right);
            --song.tail;
        }

        out[2 * i + 0] = left;
        out[2 * i + 1] = right;
    }
}

static void dataCallback(ma_device* dev, void* output, const void* input, ma_uint32 frameCount)
{
    (void)dev;
    (void)input;
    if (songReady)
    {
        render(output, frameCount);
    }
    else
    {
        memset(output, 0, (size_t)frameCount * 2 * sizeof(int16_t));
    }
}

static void freeSong(void)
{
    if (song.source != NULL)
    {
        song.source->free(song.source);
    }
    free(song.data);
    memset(&song, 0, sizeof song);
}

static bool ensureDevice(void)
{
    if (!deviceReady)
    {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_s16;
        config.playback.channels = 2;
        config.sampleRate = 0;
        config.dataCallback = dataCallback;
        if (ma_device_init(NULL, &config, &device) != MA_SUCCESS)
        {
            return false;
        }
        deviceReady = true;
    }

    if (!started)
    {
        if (ma_device_start(&device) != MA_SUCCESS)
        {
            return false;
        }
        started = true;
    }
    return true;
}

EMSCRIPTEN_KEEPALIVE
int webInit(void)
{
    return ensureDevice() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int webLoad(const char* name, const uint8_t* data, int size)
{
    if (!ensureDevice())
    {
        return 0;
    }

    songReady = false;
    freeSong();

    if (size <= 0)
    {
        return 0;
    }
    song.data = (uint8_t*)malloc((size_t)size);
    if (song.data == NULL)
    {
        return 0;
    }
    memcpy(song.data, data, (size_t)size);

    Opal_Init(&song.opl, (int)device.sampleRate);

    for (size_t i = 0; i < sizeof formats / sizeof formats[0]; ++i)
    {
        song.source = formats[i]->load(name, song.data, (size_t)size, device.sampleRate, &song.opl);
        if (song.source != NULL)
        {
            break;
        }
    }
    if (song.source == NULL)
    {
        freeSong();
        return 0;
    }

    song.tail = framesForMs(device.sampleRate, TAIL_MS);

    bool done = false;
    song.wait = song.source->step(song.source, &song.opl, &done);
    song.ended = done;
    if (done)
    {
        Opal_FlushWriteBuf(&song.opl);
    }

    songReady = true;
    return 1;
}

EMSCRIPTEN_KEEPALIVE
void webStop(void)
{
    songReady = false;
    freeSong();
    if (deviceReady && started)
    {
        ma_device_stop(&device);
        started = false;
    }
}

EMSCRIPTEN_KEEPALIVE
int webFinished(void)
{
    return (songReady && song.ended && song.wait == 0 && song.tail == 0) ? 1 : 0;
}
