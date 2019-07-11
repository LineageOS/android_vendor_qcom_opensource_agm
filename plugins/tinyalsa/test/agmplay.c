/*
** Copyright (c) 2019, The Linux Foundation. All rights reserved.
**
** Copyright 2011, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of The Android Open Source Project nor the names of
**       its contributors may be used to endorse or promote products derived
**       from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
** DAMAGE.
**/

#include <errno.h>
#include <tinyalsa/asoundlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#include <agm/agm_api.h>
#include "agmmixer.h"

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

struct riff_wave_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t wave_id;
};

struct chunk_header {
    uint32_t id;
    uint32_t sz;
};

struct chunk_fmt {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

char *audio_interface_name[] = {
    "CODEC_DMA-LPAIF_VA-TX-0",
    "CODEC_DMA-LPAIF_VA-TX-1",
    "CODEC_DMA-LPAIF_WSA-RX-0",
    "CODEC_DMA-LPAIF_WSA-RX-1",
    "MI2S-LPAIF_AXI-RX-PRIMARY",
    "MI2S-LPAIF_AXI-TX-PRIMARY",
    "TDM-LPAIF_AXI-RX-PRIMARY",
    "TDM-LPAIF_AXI-TX-PRIMARY",
    "AUXPCM-LPAIF_AXI-RX-PRIMARY",
    "AUXPCM-LPAIF_AXI-TX-PRIMARY",
};

static int close = 0;

void play_sample(FILE *file, unsigned int card, unsigned int device, unsigned int channels,
                 unsigned int rate, unsigned int bits, unsigned int period_size,
                 unsigned int period_count, unsigned int audio_intf);

void stream_close(int sig)
{
    /* allow the stream to be closed gracefully */
    signal(sig, SIG_IGN);
    close = 1;
}

int main(int argc, char **argv)
{
    FILE *file;
    struct riff_wave_header riff_wave_header;
    struct chunk_header chunk_header;
    struct chunk_fmt chunk_fmt;
    unsigned int device = 0;
    unsigned int card = 0;
    unsigned int audio_intf = 0;
    unsigned int period_size = 1024;
    unsigned int period_count = 4;
    char *filename;
    int more_chunks = 1;

    if (argc < 2) {
        printf("Usage: %s file.wav [-D card] [-d device] [-p period_size]"
                " [-n n_periods] [-o audio_intf_id]\n"
                " valid audio_intf_id :\n"
                "0 : WSA_CDC_DMA_RX_0\n"
                "1 : RX_CDC_DMA_RX_0\n"
                "2 : SLIM_0_RX\n"
                "3 : DISPLAY_PORT_RX\n"
                "4 : PRI_TDM_RX_0\n"
                "5 : SEC_TDM_RX_0\n"
                "6 : AUXPCM_RX\n"
                "7 : SEC_AUXPCM_RX\n"
                "8 : PRI_MI2S_RX\n"
                "9 : SEC_MI2S_RX\n", argv[0]);
        return 1;
    }

    filename = argv[1];
    file = fopen(filename, "rb");
    if (!file) {
        printf("Unable to open file '%s'\n", filename);
        return 1;
    }

    fread(&riff_wave_header, sizeof(riff_wave_header), 1, file);
    if ((riff_wave_header.riff_id != ID_RIFF) ||
        (riff_wave_header.wave_id != ID_WAVE)) {
        printf("Error: '%s' is not a riff/wave file\n", filename);
        fclose(file);
        return 1;
    }

    do {
        fread(&chunk_header, sizeof(chunk_header), 1, file);

        switch (chunk_header.id) {
        case ID_FMT:
            fread(&chunk_fmt, sizeof(chunk_fmt), 1, file);
            /* If the format header is larger, skip the rest */
            if (chunk_header.sz > sizeof(chunk_fmt))
                fseek(file, chunk_header.sz - sizeof(chunk_fmt), SEEK_CUR);
            break;
        case ID_DATA:
            /* Stop looking for chunks */
            more_chunks = 0;
            break;
        default:
            /* Unknown chunk, skip bytes */
            fseek(file, chunk_header.sz, SEEK_CUR);
        }
    } while (more_chunks);

    /* parse command line arguments */
    argv += 2;
    while (*argv) {
        if (strcmp(*argv, "-d") == 0) {
            argv++;
            if (*argv)
                device = atoi(*argv);
        }
        if (strcmp(*argv, "-p") == 0) {
            argv++;
            if (*argv)
                period_size = atoi(*argv);
        }
        if (strcmp(*argv, "-n") == 0) {
            argv++;
            if (*argv)
                period_count = atoi(*argv);
        }
        if (strcmp(*argv, "-D") == 0) {
            argv++;
            if (*argv)
                card = atoi(*argv);
        }
        if (strcmp(*argv, "-o") == 0) {
            argv++;
            if (*argv)
                audio_intf = atoi(*argv);
            if (audio_intf >= sizeof(audio_interface_name)/sizeof(char *)) {
                printf("Invalid audio interface index denoted by -i\n");
                fclose(file);
                return 1;
            }
        }
        if (*argv)
            argv++;
    }

    play_sample(file, card, device, chunk_fmt.num_channels, chunk_fmt.sample_rate,
                chunk_fmt.bits_per_sample, period_size, period_count, audio_intf);

    fclose(file);

    return 0;
}

int check_param(struct pcm_params *params, unsigned int param, unsigned int value,
                 char *param_name, char *param_unit)
{
    unsigned int min;
    unsigned int max;
    int is_within_bounds = 1;

    min = pcm_params_get_min(params, param);
    if (value < min) {
        printf("%s is %u%s, device only supports >= %u%s\n", param_name, value,
                param_unit, min, param_unit);
        is_within_bounds = 0;
    }

    max = pcm_params_get_max(params, param);
    if (value > max) {
        printf("%s is %u%s, device only supports <= %u%s\n", param_name, value,
                param_unit, max, param_unit);
        is_within_bounds = 0;
    }

    return is_within_bounds;
}

int sample_is_playable(unsigned int card, unsigned int device, unsigned int channels,
                        unsigned int rate, unsigned int bits, unsigned int period_size,
                        unsigned int period_count)
{
    struct pcm_params *params;
    int can_play;

    params = pcm_params_get(card, device, PCM_OUT);
    if (params == NULL) {
        printf("Unable to open PCM device %u.\n", device);
        return 0;
    }

    can_play = check_param(params, PCM_PARAM_RATE, rate, "Sample rate", "Hz");
    can_play &= check_param(params, PCM_PARAM_CHANNELS, channels, "Sample", " channels");
    can_play &= check_param(params, PCM_PARAM_SAMPLE_BITS, bits, "Bitrate", " bits");
    can_play &= check_param(params, PCM_PARAM_PERIOD_SIZE, period_size, "Period size", " frames");
    can_play &= check_param(params, PCM_PARAM_PERIODS, period_count, "Period count", " periods");

    pcm_params_free(params);

    return can_play;
}

void play_sample(FILE *file, unsigned int card, unsigned int device, unsigned int channels,
                 unsigned int rate, unsigned int bits, unsigned int period_size,
                 unsigned int period_count, unsigned int audio_intf)
{
    struct pcm_config config;
    struct pcm *pcm;
    struct mixer *mixer;
    char *buffer;
    int size;
    int num_read;
    char *intf_name = audio_interface_name[audio_intf];

    memset(&config, 0, sizeof(config));
    config.channels = channels;
    config.rate = rate;
    config.period_size = period_size;
    config.period_count = period_count;
    if (bits == 32)
        config.format = PCM_FORMAT_S32_LE;
    else if (bits == 24)
        config.format = PCM_FORMAT_S24_3LE;
    else if (bits == 16)
        config.format = PCM_FORMAT_S16_LE;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;

    mixer = mixer_open(card);
    if (!mixer) {
        printf("Failed to open mixer\n");
        return;
    }

    /* set device/audio_intf media config mixer control */
    if (set_agm_device_media_config(mixer, channels, rate, bits, intf_name)) {
        printf("Failed to set device media config\n");
        goto err_close_mixer;
    }

    /* set audio interface metadata mixer control */
    if (set_agm_audio_intf_metadata(mixer, intf_name, PLAYBACK, rate, bits)) {
        printf("Failed to set device metadata\n");
        goto err_close_mixer;
    }

    /* set audio interface metadata mixer control */
    if (set_agm_stream_metadata(mixer, device, PCM_LL_PLAYBACK, STREAM_PCM, NULL)) {
        printf("Failed to set pcm metadata\n");
        goto err_close_mixer;
    }

    /* Note:  No common metadata as of now*/

    /* connect pcm stream to audio intf */
    if (connect_agm_audio_intf_to_stream(mixer, device, intf_name, STREAM_PCM, true)) {
        printf("Failed to connect pcm to audio interface\n");
        goto err_close_mixer;
    }
/*
    if (!sample_is_playable(card, device, channels, rate, bits, period_size, period_count)) {
        return;
    }
*/
    pcm = pcm_open(card, device, PCM_OUT, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        printf("Unable to open PCM device %u (%s)\n",
                device, pcm_get_error(pcm));
        goto err_close_mixer;
    }

    size = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
    buffer = malloc(size);
    if (!buffer) {
        printf("Unable to allocate %d bytes\n", size);
        goto err_close_pcm;
    }

    printf("Playing sample: %u ch, %u hz, %u bit\n", channels, rate, bits);

    if (pcm_start(pcm) < 0) {
        printf("start error\n");
        goto err_close_pcm;
    }

    /* catch ctrl-c to shutdown cleanly */
    signal(SIGINT, stream_close);

    do {
        num_read = fread(buffer, 1, size, file);
        if (num_read > 0) {
            if (pcm_write(pcm, buffer, num_read)) {
                printf("Error playing sample\n");
                break;
            }
        }
    } while (!close && num_read > 0);

    /* connect pcm stream to audio intf */
    connect_agm_audio_intf_to_stream(mixer, device, intf_name, STREAM_PCM, false);

    free(buffer);
err_close_pcm:
    pcm_close(pcm);
err_close_mixer:
    mixer_close(mixer);
}

