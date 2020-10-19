/*
** Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above
**     copyright notice, this list of conditions and the following
**     disclaimer in the documentation and/or other materials provided
**     with the distribution.
**   * Neither the name of The Linux Foundation nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
** WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
** ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
** BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
** BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
** OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
** IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/
#define LOG_TAG "PLUGIN: AGMIO"
#include <stdio.h>
#include <sys/poll.h>

#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>

#include <agm/agm_api.h>
#include <agm/agm_list.h>
#include <snd-card-def.h>
#include "utils.h"

#define ARRAY_SIZE(a)   (sizeof(a)/sizeof(a[0]))

struct agmio_priv {
    snd_pcm_ioplug_t io;

    int card;
    int device;
/* add private variables here */
};

static int agm_io_start(snd_pcm_ioplug_t * io)
{
    AGM_LOGE("%s %d\n", __func__, __LINE__);
    return 0;
}

static int agm_io_stop(snd_pcm_ioplug_t * io)
{
    AGM_LOGE("%s %d\n", __func__, __LINE__);
    return 0;
}

static int agm_io_drain(snd_pcm_ioplug_t * io)
{
    AGM_LOGE("%s %d\n", __func__, __LINE__);
    return 0;
}

static snd_pcm_sframes_t agm_io_pointer(snd_pcm_ioplug_t * io)
{
    AGM_LOGE("%s %d\n", __func__, __LINE__);
    return -1;
}

static snd_pcm_sframes_t agm_io_transfer(snd_pcm_ioplug_t * io,
                                     const snd_pcm_channel_area_t * areas,
                                     snd_pcm_uframes_t offset,
                                     snd_pcm_uframes_t size)
{
    AGM_LOGE("%s %d\n", __func__, __LINE__);
    return size;
}

static int agm_io_prepare(snd_pcm_ioplug_t * io)
{
    AGM_LOGE("%s %d\n", __func__, __LINE__);
    return 0;
}

static int agm_io_hw_params(snd_pcm_ioplug_t * io,
                           snd_pcm_hw_params_t * params)
{
    AGM_LOGE("%s %d\n", __func__, __LINE__);
    return 0;
}

static int agm_io_sw_params(snd_pcm_ioplug_t *io, snd_pcm_sw_params_t *params)
{
    AGM_LOGE("%s %d\n", __func__, __LINE__);
    return 0;
}

static int agm_io_close(snd_pcm_ioplug_t * io)
{
    AGM_LOGE("%s %d\n", __func__, __LINE__);
    return 0;
}

static const snd_pcm_ioplug_callback_t agm_io_callback = {
    .start = agm_io_start,
    .stop = agm_io_stop,
    .pointer = agm_io_pointer,
    .drain = agm_io_drain,
    .transfer = agm_io_transfer,
    .prepare = agm_io_prepare,
    .hw_params = agm_io_hw_params,
    .sw_params = agm_io_sw_params,
    .close = agm_io_close,
};

static int agm_hw_constraint(struct agmio_priv* priv)
{
    snd_pcm_ioplug_t *io = &priv->io;
    int ret;

    static const snd_pcm_access_t access_list[] = {
        SND_PCM_ACCESS_RW_INTERLEAVED
    };
    static const unsigned int formats[] = {
        SND_PCM_FORMAT_U8,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_FORMAT_S32_LE,
        SND_PCM_FORMAT_S24_3LE,
        SND_PCM_FORMAT_S24_LE,
    };

    ret = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_ACCESS,
                                        ARRAY_SIZE(access_list),
                                        access_list);
    if (ret < 0)
        return ret;

    ret = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_FORMAT,
                                        ARRAY_SIZE(formats), formats);
    if (ret < 0)
        return ret;

    ret = snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_CHANNELS,
                                          1, 8);
    if (ret < 0)
            return ret;

    ret = snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_RATE,
                                          8000, 384000);
    if (ret < 0)
            return ret;

    ret = snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_PERIOD_BYTES,
                                          64, 122880);
    if (ret < 0)
            return ret;

    ret = snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_PERIODS,
                                          1, 8);
    if (ret < 0)
            return ret;

    return 0;
}

SND_PCM_PLUGIN_DEFINE_FUNC(agm)
{
    snd_config_iterator_t it, next;
    struct agmio_priv *priv = NULL;
    long card = 0, device = 100;
    int ret = 0;

    priv = calloc(1, sizeof(*priv));
    if (!priv)
        return -ENOMEM;

    snd_config_for_each(it, next, conf) {
        snd_config_t *n = snd_config_iterator_entry(it);
        const char *id;

        if (snd_config_get_id(n, &id) < 0)
            continue;
        if (strcmp(id, "comment") == 0 || strcmp(id, "type") == 0 || strcmp(id, "hint") == 0)
            continue;
        if (strcmp(id, "card") == 0) {
            if (snd_config_get_integer(n, &card) < 0) {
                AGM_LOGE("Invalid type for %s", id);
                ret = -EINVAL;
                goto err_free_priv;
            }
		AGM_LOGE("card id is %d\n", card);
            priv->card = card;
            continue;
        }
        if (strcmp(id, "device") == 0) {
            if (snd_config_get_integer(n, &device) < 0) {
                AGM_LOGE("Invalid type for %s", id);
                ret = -EINVAL;
                goto err_free_priv;
            }
		AGM_LOGE("device id is %d\n", device);
            priv->device = device;
            continue;
        }
    }
    priv->io.version = SND_PCM_IOPLUG_VERSION;
    priv->io.name = "AGM PCM I/O Plugin";
    priv->io.mmap_rw = 0;
    priv->io.callback = &agm_io_callback;
    priv->io.private_data = priv;

    ret = snd_pcm_ioplug_create(&priv->io, name, stream, mode);
    if (ret < 0) {
        AGM_LOGE("IO plugin create failed\n");
        goto err_free_priv;
    }

    ret = agm_hw_constraint(priv);
    if (ret < 0) {
        snd_pcm_ioplug_delete(&priv->io);
        goto err_free_priv;
    }

    *pcmp = priv->io.pcm;
    return 0;
err_free_priv:
    free(priv);
    return ret;
}

SND_PCM_PLUGIN_SYMBOL(agm);
