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
#include <sound/asound.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <tinycompress/compress_plugin.h>
#include <tinycompress/tinycompress.h>
#include <snd-card-def.h>
#include <tinyalsa/asoundlib.h>
#include "sound/compress_params.h"
#include "sound/compress_offload.h"

/* Default values */
#define COMPR_PLAYBACK_MIN_FRAGMENT_SIZE (8 * 1024)
#define COMPR_PLAYBACK_MAX_FRAGMENT_SIZE (128 * 1024)
#define COMPR_PLAYBACK_MIN_NUM_FRAGMENTS (4)
#define COMPR_PLAYBACK_MAX_NUM_FRAGMENTS (16)

struct agm_compress_priv {
    struct agm_media_config media_config;
    struct agm_buffer_config buffer_config;
    struct agm_session_config session_config;
    struct snd_compr_caps compr_cap;
    struct session_obj *handle;
    bool prepared;
    uint64_t bytes_copied; /* Copied to DSP buffer */
    uint64_t total_buf_size; /* Total buffer size */

    int64_t bytes_avail; /* avail size to write/read */

    uint64_t bytes_received;  /* from DSP */
    uint64_t bytes_read;  /* Consumed by client */
    bool start_drain;
    bool eos;

    void *client_data;
    void *card_node;
    pthread_cond_t drain_cond;
    pthread_mutex_t drain_lock;
    pthread_cond_t eos_cond;
    pthread_mutex_t eos_lock;
    pthread_mutex_t lock;
    pthread_cond_t poll_cond;
    pthread_mutex_t poll_lock;
};

static int agm_get_session_handle(struct agm_compress_priv *priv,
                                  struct session_obj **handle)
{
    if (!priv)
        return -EINVAL;

    *handle = priv->handle;
    if (NULL == *handle)
        return -EINVAL;

    return 0;
}

void agm_compress_event_cb(uint32_t session_id,
                           struct agm_event_cb_params *event_params,
                           void *client_data)
{
    struct compress_plugin *agm_compress_plugin = client_data;
    struct agm_compress_priv *priv;

    if (!agm_compress_plugin) {
        printf("%s: client_data is NULL\n", __func__);
        return;
    }
    priv = agm_compress_plugin->priv;
    if (!priv) {
        printf("%s: Private data is NULL\n", __func__);
        return;
    }
    if (!event_params) {
        printf("%s: event params is NULL\n", __func__);
        return;
    }

    pthread_mutex_lock(&priv->lock);
    printf("%s: enter: bytes_avail = %ld, event_id = %d\n", __func__,
           priv->bytes_avail, event_params->event_id);
    if (event_params->event_id == AGM_EVENT_WRITE_DONE) {
        /*
         * Write done cb is expected for every DSP write with
         * fragment size even for partial buffers
         */
        priv->bytes_avail += priv->buffer_config.size;
        if (priv->bytes_avail > priv->total_buf_size) {
            printf("%s: Error: bytes_avail %ld, total size = %ld\n",
                   __func__, priv->bytes_avail, priv->total_buf_size);
            pthread_mutex_unlock(&priv->lock);
            return;
        }

        pthread_mutex_lock(&priv->drain_lock);
        if (priv->start_drain &&
            (priv->bytes_avail == priv->total_buf_size))
            pthread_cond_signal(&priv->drain_cond);
        pthread_mutex_unlock(&priv->drain_lock);
    } else if (event_params->event_id == AGM_EVENT_READ_DONE) {
        /* Read done cb expected for every DSP read with Fragment size */
        priv->bytes_avail += priv->buffer_config.size;
        priv->bytes_received += priv->buffer_config.size;
    } else if (event_params->event_id == AGM_EVENT_EOS_RENDERED) {
        /* Unblock eos wait if all the buffers are rendered */
        pthread_mutex_lock(&priv->eos_lock);
        if (priv->eos) {
            pthread_cond_signal(&priv->eos_cond);
            priv->eos = false;
        }
        pthread_mutex_unlock(&priv->eos_lock);
    } else {
        printf("%s: error: Invalid event params id: %d\n", __func__,
           event_params->event_id);
    }
    pthread_mutex_unlock(&priv->lock);
    /* Signal Poll */
    pthread_mutex_lock(&priv->poll_lock);
    pthread_cond_signal(&priv->poll_cond);
    pthread_mutex_unlock(&priv->poll_lock);
}

int agm_compress_write(struct compress_plugin *plugin, const void *buff, size_t count)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    int ret = 0;
    int64_t size = count, buf_cnt;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    if (count > priv->total_buf_size) {
        printf("%s: Size %ld is greater than total buf size %ld\n",
               __func__, count, priv->total_buf_size);
        return -EINVAL;
    }

    /* Call prepare in the first write as write() will be called before start() */
    if (!priv->prepared) {
        ret = agm_session_prepare(handle);
        if (ret)
            return ret;
        priv->prepared = true;
    }

    pthread_mutex_lock(&priv->lock);
    ret = agm_session_write(handle, (void *)buff, &size);
    if (ret)
        goto err;

    buf_cnt = size / priv->buffer_config.size;
    if (size % priv->buffer_config.size != 0)
        buf_cnt +=1;

    /* Avalible buffer size is always multiple of fragment size */
    priv->bytes_avail -= (buf_cnt * priv->buffer_config.size);
    if (priv->bytes_avail < 0) {
        printf("%s: err: bytes_avail = %ld", __func__, priv->bytes_avail);
        ret = -EINVAL;
        goto err;
    }
    //printf("%s: count = %ld, priv->bytes_avail: %ld\n", __func__, count, priv->bytes_avail);
    priv->bytes_copied += size;
    ret = size;
err:
    pthread_mutex_unlock(&priv->lock);

    return ret;
}

int agm_compress_read(struct compress_plugin *plugin, void *buff, size_t count)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    int ret = 0, buf_cnt = 0;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    if (count > priv->bytes_avail) {
        printf("%s: Invalid requested size %ld", __func__, count);
        return -EINVAL;
    }

    ret = agm_session_read(handle, buff, &count);
    if (ret < 0)
        return ret;

    pthread_mutex_lock(&priv->lock);

    buf_cnt = count / priv->buffer_config.size;
    if (count % priv->buffer_config.size != 0)
        buf_cnt +=1;

    /* Avalible buffer size is always multiple of fragment size */
    priv->bytes_avail -= (buf_cnt * priv->buffer_config.size);
    if (priv->bytes_avail < 0)
        printf("%s: err: bytes_avail = %ld", __func__, priv->bytes_avail);

    priv->bytes_read += count;

    pthread_mutex_unlock(&priv->lock);

    return 0;
}

int agm_compress_tstamp(struct compress_plugin *plugin, struct snd_compr_tstamp *tstamp)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    int ret = 0;
    uint64_t timestamp = 0;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    tstamp->sampling_rate = priv->media_config.rate;
    tstamp->copied_total = priv->bytes_copied;

    ret = agm_get_session_time(handle, &timestamp);
    if (ret)
        return ret;

    timestamp *= tstamp->sampling_rate;
    tstamp->pcm_io_frames = timestamp/1000000;

    return 0;
}

int agm_compress_avail(struct compress_plugin *plugin, struct snd_compr_avail *avail)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    void *buff;
    size_t count;
    int ret = 0;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    agm_compress_tstamp(plugin, &avail->tstamp);

    pthread_mutex_lock(&priv->lock);
    /* Avail size is always in multiples of fragment size */
    avail->avail = priv->bytes_avail;
    printf("%s: size = %d, *avail = %d, pcm_io_frames: %d sampling_rate: %d\n", __func__,
           sizeof(struct snd_compr_avail), avail->avail, avail->tstamp.pcm_io_frames,
           avail->tstamp.sampling_rate);
    pthread_mutex_unlock(&priv->lock);

    return ret;
}

int agm_compress_get_caps(struct compress_plugin *plugin, struct snd_compr_caps *caps)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    int ret;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    if (caps) {
        memcpy(caps, &priv->compr_cap, sizeof(struct snd_compr_caps));
    } else {
        ret = -EINVAL;
        return ret;
    }

    return 0;
}

int agm_session_update_codec_config(struct agm_compress_priv *priv,
                                           struct snd_compr_params *params)
{
    struct agm_media_config *media_cfg;
    struct agm_session_config *sess_cfg;
    union snd_codec_options *copt;

    media_cfg = &priv->media_config;
    sess_cfg = &priv->session_config;
    copt = &params->codec.options;

    media_cfg->rate =  params->codec.sample_rate;
    media_cfg->channels = params->codec.ch_out;

    switch(params->codec.id){
    case SND_AUDIOCODEC_MP3:
        media_cfg->format = AGM_FORMAT_MP3;
        break;
    case SND_AUDIOCODEC_AAC:
        media_cfg->format = AGM_FORMAT_AAC;
        if (params->codec.format == SND_AUDIOSTREAMFORMAT_MP4LATM)
            sess_cfg->codec.aac_dec.aac_fmt_flag = 0x04;
        else if (params->codec.format == SND_AUDIOSTREAMFORMAT_ADIF)
            sess_cfg->codec.aac_dec.aac_fmt_flag = 0x02;
        else
            sess_cfg->codec.aac_dec.aac_fmt_flag = 0x00;
        sess_cfg->codec.aac_dec.num_channels = params->codec.ch_in;
        sess_cfg->codec.aac_dec.sample_rate = media_cfg->rate;
#ifdef COMPRESS_UAPI_DEC_HEADER
        sess_cfg->codec.aac_dec.audio_obj_type = copt->aac_dec.audio_obj_type;
        sess_cfg->codec.aac_dec.total_size_of_PCE_bits = copt->aac_dec.pce_bits_size;
#endif
        break;
    case SND_AUDIOCODEC_FLAC:
        media_cfg->format = AGM_FORMAT_FLAC;
        sess_cfg->codec.flac_dec.num_channels = params->codec.ch_in;
        sess_cfg->codec.flac_dec.sample_rate = media_cfg->rate;
        sess_cfg->codec.flac_dec.sample_size = copt->flac_dec.sample_size;
        sess_cfg->codec.flac_dec.min_blk_size = copt->flac_dec.min_blk_size;
        sess_cfg->codec.flac_dec.max_blk_size = copt->flac_dec.max_blk_size;
        sess_cfg->codec.flac_dec.min_frame_size = copt->flac_dec.min_frame_size;
        sess_cfg->codec.flac_dec.max_frame_size = copt->flac_dec.max_frame_size;
        break;
    case SND_AUDIOCODEC_ALAC:
        media_cfg->format = AGM_FORMAT_ALAC;
        sess_cfg->codec.alac_dec.num_channels = params->codec.ch_in;
        sess_cfg->codec.alac_dec.sample_rate = media_cfg->rate;
        sess_cfg->codec.alac_dec.frame_length = copt->alac_dec.frame_length;
        sess_cfg->codec.alac_dec.compatible_version = copt->alac_dec.compatible_version;
        sess_cfg->codec.alac_dec.bit_depth = copt->alac_dec.bit_depth;
        sess_cfg->codec.alac_dec.pb = copt->alac_dec.pb;
        sess_cfg->codec.alac_dec.mb = copt->alac_dec.mb;
        sess_cfg->codec.alac_dec.kb = copt->alac_dec.kb;
        sess_cfg->codec.alac_dec.max_run = copt->alac_dec.max_run;
        sess_cfg->codec.alac_dec.max_frame_bytes = copt->alac_dec.max_frame_bytes;
        sess_cfg->codec.alac_dec.avg_bit_rate = copt->alac_dec.avg_bit_rate;
        sess_cfg->codec.alac_dec.channel_layout_tag = copt->alac_dec.channel_layout_tag;
        break;
    case SND_AUDIOCODEC_APE:
        media_cfg->format = AGM_FORMAT_APE;
        sess_cfg->codec.ape_dec.num_channels = params->codec.ch_in;
        sess_cfg->codec.ape_dec.sample_rate = media_cfg->rate;
        sess_cfg->codec.ape_dec.bit_width = copt->ape_dec.bits_per_sample;
        sess_cfg->codec.ape_dec.compatible_version = copt->ape_dec.compatible_version;
        sess_cfg->codec.ape_dec.compression_level = copt->ape_dec.compression_level;
        sess_cfg->codec.ape_dec.format_flags = copt->ape_dec.format_flags;
        sess_cfg->codec.ape_dec.blocks_per_frame = copt->ape_dec.blocks_per_frame;
        sess_cfg->codec.ape_dec.final_frame_blocks = copt->ape_dec.final_frame_blocks;
        sess_cfg->codec.ape_dec.total_frames = copt->ape_dec.total_frames;
        sess_cfg->codec.ape_dec.seek_table_present = copt->ape_dec.seek_table_present;
        break;
    case SND_AUDIOCODEC_WMA:
        media_cfg->format = AGM_FORMAT_WMASTD;
        sess_cfg->codec.wma_dec.fmt_tag = params->codec.format;
        sess_cfg->codec.wma_dec.num_channels = params->codec.ch_in;
        sess_cfg->codec.wma_dec.sample_rate = media_cfg->rate;
#ifdef COMPRESS_UAPI_DEC_HEADER
        sess_cfg->codec.wma_dec.avg_bytes_per_sec = copt->wma_dec.avg_bit_rate/8;
        sess_cfg->codec.wma_dec.blk_align = copt->wma_dec.super_block_align;
        sess_cfg->codec.wma_dec.bits_per_sample = copt->wma_dec.bits_per_sample;
        sess_cfg->codec.wma_dec.channel_mask = copt->wma_dec.channelmask;
        sess_cfg->codec.wma_dec.enc_options = copt->wma_dec.encodeopt;
#endif
        break;
    case SND_AUDIOCODEC_WMA_PRO:
        media_cfg->format = AGM_FORMAT_WMAPRO;
        sess_cfg->codec.wmapro_dec.fmt_tag = params->codec.format;
        sess_cfg->codec.wmapro_dec.num_channels = params->codec.ch_in;
        sess_cfg->codec.wmapro_dec.sample_rate = media_cfg->rate;
#ifdef COMPRESS_UAPI_DEC_HEADER
        sess_cfg->codec.wmapro_dec.avg_bytes_per_sec = copt->wma_dec.avg_bit_rate/8;
        sess_cfg->codec.wmapro_dec.blk_align = copt->wma_dec.super_block_align;
        sess_cfg->codec.wmapro_dec.bits_per_sample = copt->wma_dec.bits_per_sample;
        sess_cfg->codec.wmapro_dec.channel_mask = copt->wma_dec.channelmask;
        sess_cfg->codec.wmapro_dec.enc_options = copt->wma_dec.encodeopt;
        sess_cfg->codec.wmapro_dec.advanced_enc_option = copt->wma_dec.encodeopt1;
        sess_cfg->codec.wmapro_dec.advanced_enc_option2 = copt->wma_dec.encodeopt2;
#endif
        break;
    case SND_AUDIOCODEC_VORBIS:
        media_cfg->format = AGM_FORMAT_VORBIS;
        break;
    default:
        break;
    }
    printf("%s: format = %d rate = %d, channels = %d\n", __func__,
           media_cfg->format, media_cfg->rate, media_cfg->channels);
    return 0;
}

int agm_compress_set_params(struct compress_plugin *plugin, struct snd_compr_params *params)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct agm_buffer_config *buf_cfg;
    struct agm_session_config *sess_cfg;
    struct session_obj *handle;
    int ret = 0;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    buf_cfg = &priv->buffer_config;
    buf_cfg->count = params->buffer.fragments;
    buf_cfg->size = params->buffer.fragment_size;
    priv->total_buf_size = buf_cfg->size * buf_cfg->count;

    sess_cfg = &priv->session_config;
    if (sess_cfg->dir == RX)
        priv->bytes_avail = priv->total_buf_size;
    else
        priv->bytes_avail = 0;

    sess_cfg->start_threshold = 0;
    sess_cfg->stop_threshold = 0;
    sess_cfg->data_mode = AGM_DATA_NON_BLOCKING;
    /* Populate each codec format specific params */
    ret = agm_session_update_codec_config(priv, params);
    if (ret)
        return ret;

    ret = agm_session_set_config(priv->handle, sess_cfg,
                                 &priv->media_config, buf_cfg);
    if (ret)
        return ret;

    printf("%s: exit fragments cnt = %d size = %ld\n", __func__,
           buf_cfg->count, buf_cfg->size);
    return ret;
}

static int agm_compress_start(struct compress_plugin *plugin)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    int ret;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    return agm_session_start(handle);
}

static int agm_compress_stop(struct compress_plugin *plugin)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    int ret;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    /* Unlock drain if its waiting for EOS rendered */
    pthread_mutex_lock(&priv->eos_lock);
    if (priv->eos) {
        pthread_cond_signal(&priv->eos_cond);
        priv->eos = false;
    }
    pthread_mutex_unlock(&priv->eos_lock);

    ret = agm_session_stop(handle);
    if (ret)
        return ret;

    /* stop will reset all the buffers and it called during seek also */
    /*TODO: reset bytes_avial and other local variables */
    priv->bytes_avail = priv->total_buf_size;
    priv->bytes_copied = 0;

    return ret;
}

static int agm_compress_pause(struct compress_plugin *plugin)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    int ret;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    return agm_session_pause(handle);
}

static int agm_compress_resume(struct compress_plugin *plugin)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    int ret;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    return agm_session_resume(handle);
}

static int agm_compress_drain(struct compress_plugin *plugin)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    int ret;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    printf("%s: priv->bytes_avail = %ld,  priv->total_buf_size = %ld\n",
           __func__, priv->bytes_avail, priv->total_buf_size);
    /* No need to wait for all buffers to be consumed to issue EOS as
     * write and EOS cmds are sequential
     */
    /* TODO: how to handle wake up in SSR scenario */
    pthread_mutex_lock(&priv->eos_lock);
    priv->eos = true;
    ret = agm_session_eos(handle);
    if (ret) {
        printf("%s: EOS fail\n", __func__);
        pthread_mutex_unlock(&priv->eos_lock);
        return ret;
    }
    pthread_cond_wait(&priv->eos_cond, &priv->eos_lock);
    printf("%s: out of eos wait\n", __func__);
    pthread_mutex_unlock(&priv->eos_lock);

    return 0;
}

static int agm_compress_partial_drain(struct compress_plugin *plugin)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    int ret;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    /* TODO: include gapless playback logic */
    return 0;
}

static int agm_compress_next_track(struct compress_plugin *plugin)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    int ret;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    /* TODO: include gapless playback logic */
    return 0;
}

static int agm_compress_poll(struct compress_plugin *plugin,
                             struct pollfd *fds, nfds_t nfds,
                             int timeout)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    struct timespec poll_ts;
    int ret = 0;

    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return ret;

    clock_gettime(CLOCK_REALTIME, &poll_ts);
    poll_ts.tv_sec += timeout/1000;
    /* Unblock poll wait if avail bytes to write/read is more than one fragment */
    pthread_mutex_lock(&priv->poll_lock);
    ret = pthread_cond_timedwait(&priv->poll_cond, &priv->poll_lock, &poll_ts);
    pthread_mutex_unlock(&priv->poll_lock);

    clock_gettime(CLOCK_REALTIME, &poll_ts);

    if (ret == ETIMEDOUT) {
        /* Poll() expects 0 return value in case of timeout */
        ret = 0;
    } else {
        fds->revents |= POLLOUT;
        ret = POLLOUT;
    }
    return ret;
}

void agm_compress_close(struct compress_plugin *plugin)
{
    struct agm_compress_priv *priv = plugin->priv;
    struct session_obj *handle;
    int ret = 0;

    printf("%s: free \n", __func__);
    ret = agm_get_session_handle(priv, &handle);
    if (ret)
        return;

    ret = agm_session_close(handle);
    if (ret)
        printf("%s: agm_session_close failed \n", __func__);

    snd_card_def_put_card(priv->card_node);
    /* Unblock eos wait if eos-rendered event cb has not been called */
    pthread_mutex_lock(&priv->eos_lock);
    if (priv->eos) {
        pthread_cond_signal(&priv->eos_cond);
        priv->eos = false;
    }
    pthread_mutex_unlock(&priv->eos_lock);
    /* Make sure callbacks are not running at this point */
    free(plugin->priv);
    free(plugin);

    return;
}

struct compress_plugin_ops agm_compress_ops = {
    .close = agm_compress_close,
    .get_caps = agm_compress_get_caps,
    .set_params = agm_compress_set_params,
    .avail = agm_compress_avail,
    .tstamp = agm_compress_tstamp,
    .write = agm_compress_write,
    .read = agm_compress_read,
    .start = agm_compress_start,
    .stop = agm_compress_stop,
    .pause = agm_compress_pause,
    .resume = agm_compress_resume,
    .drain = agm_compress_drain,
    .partial_drain = agm_compress_partial_drain,
    .next_track = agm_compress_next_track,
    .poll = agm_compress_poll,
};

static int agm_populate_codec_caps(struct agm_compress_priv *priv)
{
    priv->compr_cap.direction = SND_COMPRESS_PLAYBACK;
    priv->compr_cap.min_fragment_size =
                    COMPR_PLAYBACK_MIN_FRAGMENT_SIZE;
    priv->compr_cap.max_fragment_size =
                    COMPR_PLAYBACK_MAX_FRAGMENT_SIZE;
    priv->compr_cap.min_fragments =
                    COMPR_PLAYBACK_MIN_NUM_FRAGMENTS;
    priv->compr_cap.max_fragments =
                    COMPR_PLAYBACK_MAX_NUM_FRAGMENTS;
    priv->compr_cap.codecs[0] = SND_AUDIOCODEC_MP3;
    priv->compr_cap.codecs[1] = SND_AUDIOCODEC_AAC;
    priv->compr_cap.codecs[2] = SND_AUDIOCODEC_WMA;
    priv->compr_cap.codecs[3] = SND_AUDIOCODEC_FLAC;
    priv->compr_cap.codecs[4] = SND_AUDIOCODEC_VORBIS;
    priv->compr_cap.codecs[5] = SND_AUDIOCODEC_ALAC;
    priv->compr_cap.codecs[6] = SND_AUDIOCODEC_APE;
    priv->compr_cap.num_codecs = 7;

    return 0;
};

COMPRESS_PLUGIN_OPEN_FN(agm_compress_plugin)
{
    struct compress_plugin *agm_compress_plugin;
    struct agm_compress_priv *priv;
    struct session_obj *handle;
    int ret = 0, session_id = device;
    int is_playback = 0, is_capture = 0, is_hostless = 0;
    void *card_node, *compr_node;

    printf("%s: session_id: %d \n", __func__, device);
    agm_compress_plugin = calloc(1, sizeof(struct compress_plugin));
    if (!agm_compress_plugin)
        return -ENOMEM;

    priv = calloc(1, sizeof(struct agm_compress_priv));
    if (!priv) {
        ret = -ENOMEM;
        goto err_plugin_free;
    }

    card_node = snd_card_def_get_card(card);
    if (!card_node) {
        ret = -EINVAL;
        goto err_priv_free;
    }

    compr_node = snd_card_def_get_node(card_node, device, SND_NODE_TYPE_COMPR);
    if (!compr_node) {
        ret = -EINVAL;
        goto err_card_put;
    }

    agm_compress_plugin->card = card;
    agm_compress_plugin->ops = &agm_compress_ops;
    agm_compress_plugin->node = compr_node;
    agm_compress_plugin->priv = priv;
    priv->card_node = card_node;

    ret = snd_card_def_get_int(agm_compress_plugin->node, "playback", &is_playback);
    if (ret)
       goto err_card_put;

    ret = snd_card_def_get_int(agm_compress_plugin->node, "capture", &is_capture);
    if (ret)
       goto err_card_put;

    ret = snd_card_def_get_int(agm_compress_plugin->node, "hostless", &is_hostless);
    if (ret)
       goto err_card_put;

    priv->session_config.is_hostless = !!is_hostless;
    priv->session_config.dir = (flags & COMPRESS_IN) ? RX : TX;

    if ((priv->session_config.dir == RX) && !is_playback) {
        printf("%s: Playback is supported for device %d \n", __func__, device);
        goto err_card_put;
    }
    if ((priv->session_config.dir == TX) && !is_capture) {
        printf("%s: Capture is supported for device %d \n", __func__, device);
        goto err_card_put;
    }

    ret = agm_session_open(session_id, &handle);
    if (ret)
        goto err_card_put;

    ret = agm_session_register_cb(session_id, &agm_compress_event_cb,
                                  AGM_EVENT_DATA_PATH, agm_compress_plugin);
    if (ret)
        goto err_sess_cls;

    agm_populate_codec_caps(priv);
    priv->handle = handle;
    *plugin = agm_compress_plugin;
    pthread_mutex_init(&priv->lock, (const pthread_mutexattr_t *) NULL);
    pthread_mutex_init(&priv->eos_lock, (const pthread_mutexattr_t *) NULL);
    pthread_mutex_init(&priv->drain_lock, (const pthread_mutexattr_t *) NULL);
    pthread_mutex_init(&priv->poll_lock, (const pthread_mutexattr_t *) NULL);

    return 0;

err_sess_cls:
    agm_session_close(handle);
err_card_put:
    snd_card_def_put_card(card_node);
err_priv_free:
    free(priv);
err_plugin_free:
    free(agm_compress_plugin);
    return ret;
}
