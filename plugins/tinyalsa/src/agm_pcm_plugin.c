/*
** Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include <agm/agm_api.h>
#include <errno.h>
#include <limits.h>
#include <linux/ioctl.h>
#include <sys/time.h>
#include <sound/asound.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <tinyalsa/pcm_plugin.h>
#include <snd-card-def.h>
#include <tinyalsa/asoundlib.h>

/* 2 words of uint32_t = 64 bits of mask */
#define PCM_MASK_SIZE (2)
#define PCM_PLUGIN_EOS_TIMEOUT 1 // in seconds

struct agm_pcm_priv {
    struct agm_media_config *media_config;
    struct agm_buffer_config *buffer_config;
    struct agm_session_config *session_config;
    uint64_t handle;
    void *card_node;
    pthread_cond_t eos_cond;
    pthread_mutex_t eos_lock;
    bool eos_cmd_sent;
    int session_id;
};

struct pcm_plugin_hw_constraints agm_pcm_constrs = {
    .access = SNDRV_PCM_ACCESS_RW_INTERLEAVED |
              SNDRV_PCM_ACCESS_RW_NONINTERLEAVED,
    .format = SNDRV_PCM_FORMAT_S16_LE |
              SNDRV_PCM_FORMAT_S24_LE |
              SNDRV_PCM_FORMAT_S24_3LE |
              SNDRV_PCM_FORMAT_S32_LE,
    .bit_width = {
        .min = 16,
        .max = 32,
    },
    .channels = {
        .min = 1,
        .max = 8,
    },
    .rate = {
        .min = 8000,
        .max = 384000,
    },
    .periods = {
        .min = 2,
        .max = 8,
    },
    .period_bytes = {
        .min = 128,
        .max = 122880,
    },
};

static inline struct snd_interval *param_to_interval(struct snd_pcm_hw_params *p, int n)
{
    return &(p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL]);
}

static inline int param_is_interval(int p)
{
    return (p >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL) &&
        (p <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL);
}

static unsigned int param_get_int(struct snd_pcm_hw_params *p, int n)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        if (i->integer)
            return i->max;
    }
    return 0;
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
    return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static inline int param_is_mask(int p)
{
    return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
        (p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline int snd_mask_val(const struct snd_mask *mask)
{
	int i;
	for (i = 0; i < PCM_MASK_SIZE; i++) {
		if (mask->bits[i])
			return ffs(mask->bits[i]) + (i << 5);
	}
	return 0;
}

static unsigned int agm_format_to_bits(enum pcm_format format)
{
    switch (format) {
    case AGM_FORMAT_PCM_S32_LE:
    case AGM_FORMAT_PCM_S24_LE:
        return 32;
    case AGM_FORMAT_PCM_S24_3LE:
        return 24;
    default:
    case AGM_FORMAT_PCM_S16_LE:
        return 16;
    };
}

static enum agm_media_format alsa_to_agm_format(int format)
{
    switch (format) {
    case SNDRV_PCM_FORMAT_S32_LE:
        return AGM_FORMAT_PCM_S32_LE;
    case SNDRV_PCM_FORMAT_S8:
        return AGM_FORMAT_PCM_S8;
    case SNDRV_PCM_FORMAT_S24_3LE:
        return AGM_FORMAT_PCM_S24_3LE;
    case SNDRV_PCM_FORMAT_S24_LE:
        return AGM_FORMAT_PCM_S24_LE;
    default:
    case SNDRV_PCM_FORMAT_S16_LE:
        return AGM_FORMAT_PCM_S16_LE;
    };
}

static enum agm_media_format param_get_mask_val(struct snd_pcm_hw_params *p, int n)
{
    if (param_is_mask(n)) {
        struct snd_mask *m = param_to_mask(p, n);
        int val = snd_mask_val(m);

        return alsa_to_agm_format(val);
    }
    return 0;
}

static int agm_get_session_handle(struct agm_pcm_priv *priv,
                                  uint64_t *handle)
{
    if (!priv)
        return -EINVAL;

    *handle = priv->handle;
    if (NULL == *handle)
        return -EINVAL;

    return 0;
}

void agm_pcm_event_cb(uint32_t session_id,
                           struct agm_event_cb_params *event_params,
                           void *client_data)
{
    struct pcm_plugin *agm_pcm_plugin = client_data;
    struct agm_pcm_priv *priv;

    if (!agm_pcm_plugin) {
        printf("%s: client_data is NULL\n", __func__);
        return;
    }
    priv = agm_pcm_plugin->priv;
    if (!priv) {
        printf("%s: Private data is NULL\n", __func__);
        return;
    }
    if (!event_params) {
        printf("%s: event params is NULL\n", __func__);
        return;
    }
    if (event_params->event_id == AGM_EVENT_EOS_RENDERED) {
        pthread_mutex_lock(&priv->eos_lock);
        pthread_cond_signal(&priv->eos_cond);
        pthread_mutex_unlock(&priv->eos_lock);
    } else {
        printf("%s: error: Invalid event params id: %d\n", __func__,
           event_params->event_id);
    }
}

static int agm_pcm_hw_params(struct pcm_plugin *plugin,
                             struct snd_pcm_hw_params *params)
{
    struct agm_pcm_priv *priv = plugin->priv;
    struct agm_media_config *media_config;
    struct agm_buffer_config *buffer_config;
    uint64_t handle;
    int ret = 0;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    media_config = priv->media_config;
    buffer_config = priv->buffer_config;

    media_config->rate =  param_get_int(params, SNDRV_PCM_HW_PARAM_RATE);
    media_config->channels = param_get_int(params, SNDRV_PCM_HW_PARAM_CHANNELS);
    media_config->format = param_get_mask_val(params, SNDRV_PCM_HW_PARAM_FORMAT);

    buffer_config->count = param_get_int(params, SNDRV_PCM_HW_PARAM_PERIODS);
    buffer_config->size = param_get_int(params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);

    return 0;
}

static int agm_pcm_sw_params(struct pcm_plugin *plugin,
                             struct snd_pcm_sw_params *sparams)
{
    struct agm_pcm_priv *priv = plugin->priv;
    struct agm_session_config *session_config = NULL;
    uint64_t handle = 0;
    int ret = 0, is_hostless = 0;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    session_config = priv->session_config;

    snd_card_def_get_int(plugin->node, "hostless", &is_hostless);

    session_config->dir = (plugin->mode == 0) ? RX : TX;
    session_config->is_hostless = !!is_hostless;
    session_config->start_threshold = (uint32_t)sparams->start_threshold;
    session_config->stop_threshold = (uint32_t)sparams->stop_threshold;

    ret = agm_session_set_config(priv->handle, session_config,
                                 priv->media_config, priv->buffer_config);
    return ret;
}

static int agm_pcm_sync_ptr(struct pcm_plugin *plugin,
                            struct snd_pcm_sync_ptr *sync_ptr)
{
    struct agm_pcm_priv *priv = plugin->priv;
    uint64_t handle;

    if (!priv)
        return -EINVAL;

    handle = priv->handle;
    if (!handle)
        return -EINVAL;

    /* TODO : Add AGM API call */
    return 0;
}

static int agm_pcm_writei_frames(struct pcm_plugin *plugin, struct snd_xferi *x)
{
    struct agm_pcm_priv *priv = plugin->priv;
    uint64_t handle;
    void *buff;
    size_t count;
    int ret = 0;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    buff = x->buf;
    count = x->frames * (priv->media_config->channels *
            agm_format_to_bits(priv->media_config->format) / 8);

    return agm_session_write(handle, buff, &count);
}

static int agm_pcm_readi_frames(struct pcm_plugin *plugin, struct snd_xferi *x)
{
    struct agm_pcm_priv *priv = plugin->priv;
    uint64_t handle;
    void *buff;
    size_t count;
    int ret = 0;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    buff = x->buf;
    count = x->frames * (priv->media_config->channels *
            agm_format_to_bits(priv->media_config->format) / 8);

    return agm_session_read(handle, buff, &count);
}

static int agm_pcm_ttstamp(struct pcm_plugin *plugin, int *tstamp)
{
    struct agm_pcm_priv *priv = plugin->priv;
    uint64_t handle;
    int ret = 0;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    /* TODO : Add AGM API call */
    return 0;
}

static int agm_pcm_prepare(struct pcm_plugin *plugin)
{
    uint64_t handle;
    struct agm_pcm_priv *priv = plugin->priv;
    int ret = 0;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    return agm_session_prepare(handle);
}

static int agm_pcm_start(struct pcm_plugin *plugin)
{
    struct agm_pcm_priv *priv = plugin->priv;
    uint64_t handle;
    int ret;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    return agm_session_start(handle);
}

static void agm_pcm_eos(struct pcm_plugin *plugin, uint64_t handle)
{
    struct agm_pcm_priv *priv = plugin->priv;
    int ret = 0;
    struct timespec eos_ts;

    /*
     * closing sequence can be either agm_pcm_close() or
     * agm_pcm_drop() -> gm_pcm_close() but we should send
     * eos only once so below check is needed.
     */
    if (priv->eos_cmd_sent)
        return;

    clock_gettime(CLOCK_REALTIME, &eos_ts);
    eos_ts.tv_sec += PCM_PLUGIN_EOS_TIMEOUT;

    pthread_mutex_lock(&priv->eos_lock);
    ret = agm_session_eos(handle);
    if (ret)
        printf("%s: EOS cmd fail\n", __func__);
    else
        pthread_cond_timedwait(&priv->eos_cond, &priv->eos_lock, &eos_ts);
    pthread_mutex_unlock(&priv->eos_lock);
    priv->eos_cmd_sent = true;
}

static int agm_pcm_drop(struct pcm_plugin *plugin)
{
    struct agm_pcm_priv *priv = plugin->priv;
    uint64_t handle;
    int ret;
    struct timespec eos_ts;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    agm_pcm_eos(plugin, handle);

    return agm_session_stop(handle);
}

static int agm_pcm_close(struct pcm_plugin *plugin)
{
    struct agm_pcm_priv *priv = plugin->priv;
    uint64_t handle;
    int ret = 0;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    agm_pcm_eos(plugin, handle);
    ret = agm_session_register_cb(priv->session_id, NULL,
                                  AGM_EVENT_DATA_PATH, plugin);
    ret = agm_session_close(handle);

    snd_card_def_put_card(priv->card_node);
    free(priv->buffer_config);
    free(priv->media_config);
    free(priv->session_config);
    free(plugin->priv);
    free(plugin);

    return ret;
}

struct pcm_plugin_ops agm_pcm_ops = {
    .close = agm_pcm_close,
    .hw_params = agm_pcm_hw_params,
    .sw_params = agm_pcm_sw_params,
    .sync_ptr = agm_pcm_sync_ptr,
    .writei_frames = agm_pcm_writei_frames,
    .readi_frames = agm_pcm_readi_frames,
    .ttstamp = agm_pcm_ttstamp,
    .prepare = agm_pcm_prepare,
    .start = agm_pcm_start,
    .drop = agm_pcm_drop,
};

PCM_PLUGIN_OPEN_FN(agm_pcm_plugin)
{
    struct pcm_plugin *agm_pcm_plugin;
    struct agm_pcm_priv *priv;
    struct agm_session_config *session_config;
    struct agm_media_config *media_config;
    struct agm_buffer_config *buffer_config;
    uint64_t handle;
    int ret = 0, session_id = device;
    void *card_node, *pcm_node;

    agm_pcm_plugin = calloc(1, sizeof(struct pcm_plugin));
    if (!agm_pcm_plugin)
        return -ENOMEM;

    priv = calloc(1, sizeof(struct agm_pcm_priv));
    if (!priv) {
        ret = -ENOMEM;
        goto err_plugin_free;
    }

    media_config = calloc(1, sizeof(struct agm_media_config));
    if (!media_config) {
        ret = -ENOMEM;
        goto err_priv_free;
    }

    buffer_config = calloc(1, sizeof(struct agm_buffer_config));
    if (!buffer_config) {
        ret = -ENOMEM;
        goto err_media_free;
    }

    session_config = calloc(1, sizeof(struct agm_session_config));
    if (!session_config) {
        ret = -ENOMEM;
        goto err_buf_free;
    }

    card_node = snd_card_def_get_card(card);
    if (!card_node) {
        ret = -EINVAL;
        goto err_session_free;
    }

    pcm_node = snd_card_def_get_node(card_node, device, SND_NODE_TYPE_PCM);
    if (!pcm_node) {
        ret = -EINVAL;
        goto err_card_put;
    }

    agm_pcm_plugin->card = card;
    agm_pcm_plugin->ops = &agm_pcm_ops;
    agm_pcm_plugin->node = pcm_node;
    agm_pcm_plugin->mode = mode;
    agm_pcm_plugin->constraints = &agm_pcm_constrs;
    agm_pcm_plugin->priv = priv;

    priv->media_config = media_config;
    priv->buffer_config = buffer_config;
    priv->session_config = session_config;
    priv->card_node = card_node;
    priv->session_id = session_id;

    ret = agm_session_open(session_id, &handle);
    if (ret)
        goto err_card_put;

    ret = agm_session_register_cb(session_id, &agm_pcm_event_cb,
                                  AGM_EVENT_DATA_PATH, agm_pcm_plugin);
    if (ret)
        goto err_sess_cls;

    priv->handle = handle;
    *plugin = agm_pcm_plugin;
    pthread_mutex_init(&priv->eos_lock, (const pthread_mutexattr_t *) NULL);

    return 0;

err_sess_cls:
    agm_session_close(handle);
err_card_put:
    snd_card_def_put_card(card_node);
err_session_free:
    free(session_config);
err_buf_free:
    free(buffer_config);
err_media_free:
    free(media_config);
err_priv_free:
    free(priv);
err_plugin_free:
    free(agm_pcm_plugin);
    return ret;
}
