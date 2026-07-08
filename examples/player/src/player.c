/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

// Streams OPL music through opal to the default audio device. Each format lives in its
// own format_*.c behind the MusicFormat interface. Supported: DRO, HSC, IMF.
//
// Usage:
//     player [--rate <hz>] <song>

#include "opal/opal.h"
#include "miniaudio/miniaudio.h"

#include "format_dro.h"
#include "format_hsc.h"
#include "format_imf.h"
#include "player_format.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAIL_MS 250 // tail after song end so releases ring out

static const MusicFormat* const formats[] = {&droFormat, &hscFormat, &imfFormat};

typedef struct
{
    Opal opl;
    MusicSource* source;
    uint64_t wait;
    uint64_t tail;
    bool ended;
    bool signaled;
    ma_event finished;
} Player;

// Audio thread callback.
static void dataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount)
{
    Player* p = (Player*)device->pUserData;
    int16_t* out = (int16_t*)output;
    (void)input;

    for (ma_uint32 i = 0; i < frameCount; ++i)
    {
        int16_t left = 0;
        int16_t right = 0;

        while (p->wait == 0 && !p->ended)
        {
            bool done = false;
            p->wait += p->source->step(p->source, &p->opl, &done);
            if (done)
            {
                p->ended = true;
                Opal_FlushWriteBuf(&p->opl);
            }
        }

        if (p->wait > 0)
        {
            Opal_Sample(&p->opl, &left, &right);
            --p->wait;
        }
        else if (p->tail > 0) // tail: keep sampling for releases
        {
            Opal_Sample(&p->opl, &left, &right);
            --p->tail;
        }
        else if (!p->signaled)
        {
            p->signaled = true;
            ma_event_signal(&p->finished);
        }

        out[2 * i + 0] = left;
        out[2 * i + 1] = right;
    }
}

static uint8_t* readFile(const char* path, size_t* outSize)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL)
    {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return NULL;
    }

    const long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }

    uint8_t* buffer = (uint8_t*)malloc((size_t)size);
    if (buffer == NULL)
    {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)size, file) != (size_t)size)
    {
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);
    *outSize = (size_t)size;
    return buffer;
}

static void usage(const char* argv0)
{
    fprintf(stderr, "usage: %s [--rate <hz>] <song>\n", argv0);
    fprintf(stderr, "  --rate <hz>  force the IMF replay rate (e.g. 280, 560, 700)\n");
    fprintf(stderr, "               default: .wlf = 700 Hz, .imf = 560 Hz (ignored by other formats)\n");
}

int main(int argc, char** argv)
{
    const char* songPath = NULL;
    uint32_t rateOverride = 0;
    for (int i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];
        if (strcmp(arg, "--rate") == 0 && i + 1 < argc)
        {
            const char* rateValue = argv[++i];
            char* end = NULL;
            unsigned long hz = strtoul(rateValue, &end, 10);
            if (end == rateValue || *end != '\0' || hz == 0 || hz > 0xFFFF)
            {
                fprintf(stderr, "player: invalid --rate value '%s' (expected 1..65535 Hz)\n", rateValue);
                return EXIT_FAILURE;
            }
            rateOverride = (uint32_t)hz;
            continue;
        }
        else if (strncmp(arg, "--rate=", 7) == 0)
        {
            const char* rateValue = arg + 7;
            char* end = NULL;
            unsigned long hz = strtoul(rateValue, &end, 10);
            if (end == rateValue || *end != '\0' || hz == 0 || hz > 0xFFFF)
            {
                fprintf(stderr, "player: invalid --rate value '%s' (expected 1..65535 Hz)\n", rateValue);
                return EXIT_FAILURE;
            }
            rateOverride = (uint32_t)hz;
            continue;
        }
        else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
        {
            usage(argv[0]);
            return EXIT_SUCCESS;
        }
        else if (arg[0] == '-' && arg[1] != '\0')
        {
            fprintf(stderr, "player: unknown option '%s'\n", arg);
            usage(argv[0]);
            return EXIT_FAILURE;
        }
        else if (songPath == NULL)
        {
            songPath = arg;
            continue;
        }
        else
        {
            fprintf(stderr, "player: unexpected extra argument '%s'\n", arg);
            return EXIT_FAILURE;
        }
    }

    if (songPath == NULL)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    size_t fileSize = 0;
    uint8_t* file = readFile(songPath, &fileSize);
    if (file == NULL)
    {
        fprintf(stderr, "player: cannot read '%s'\n", songPath);
        return EXIT_FAILURE;
    }

    imfSetRate(rateOverride); // 0 uses ext default for IMF. Other formats ignore.

    // Must outlive device init. Used in pUserData.
    Player player = {0};

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = 2;
    config.sampleRate = 0; // 0 = the device's native rate; opal resamples its 49716 Hz output to match
    config.dataCallback = dataCallback;
    config.pUserData = &player;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS)
    {
        fprintf(stderr, "player: failed to open an audio playback device\n");
        free(file);
        return EXIT_FAILURE;
    }

    Opal_Init(&player.opl, (int)device.sampleRate);

    // Probe formats in order. First match wins. A format may program the chip here.
    const MusicFormat* format = NULL;
    for (size_t i = 0; i < sizeof formats / sizeof formats[0]; ++i)
    {
        player.source = formats[i]->load(songPath, file, fileSize, device.sampleRate, &player.opl);
        if (player.source != NULL)
        {
            format = formats[i];
            break;
        }
    }
    if (player.source == NULL)
    {
        fprintf(stderr, "player: '%s' is not a recognized format (DRO, HSC, IMF)\n", songPath);
        ma_device_uninit(&device);
        free(file);
        return EXIT_FAILURE;
    }

    player.tail = framesForMs(device.sampleRate, TAIL_MS);

    // Prime first event. Opening writes must land before first sample.
    {
        bool primedDone = false;
        player.wait = player.source->step(player.source, &player.opl, &primedDone);
        player.ended = primedDone;
        if (primedDone)
        {
            Opal_FlushWriteBuf(&player.opl);
        }
    }

    if (ma_event_init(&player.finished) != MA_SUCCESS)
    {
        fprintf(stderr, "player: failed to create the completion event\n");
        player.source->free(player.source);
        ma_device_uninit(&device);
        free(file);
        return EXIT_FAILURE;
    }

    printf("player: %s | %s | %u Hz\n", songPath, format->name, device.sampleRate);
    if (format == &imfFormat)
    {
        printf("player: IMF replay rate %u Hz (%s)\n", imfRate(), rateOverride ? "forced" : "by extension");
    }
    printf("player: playing... (Ctrl+C to stop)\n");

    if (ma_device_start(&device) != MA_SUCCESS)
    {
        fprintf(stderr, "player: failed to start playback\n");
        ma_event_uninit(&player.finished);
        player.source->free(player.source);
        ma_device_uninit(&device);
        free(file);
        return EXIT_FAILURE;
    }

    ma_event_wait(&player.finished);

    ma_device_uninit(&device); // stop device and join thread before freeing referenced data
    Opal_FlushWriteBuf(&player.opl);
    ma_event_uninit(&player.finished);
    player.source->free(player.source);
    free(file);

    printf("player: done\n");
    return EXIT_SUCCESS;
}
