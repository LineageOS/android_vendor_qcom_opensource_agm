/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOGTAG "AGM: graph"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include "graph.h"
#include "graph_module.h"
#include "metadata.h"
#include "utils.h"
#include "gsl_intf.h"

#define DEVICE_RX 0
#define DEVICE_TX 1
#define MONO 1

/* TODO: remove this later after including in gecko header files */
#define PARAM_ID_SOFT_PAUSE_START 0x800102e
#define PARAM_ID_SOFT_PAUSE_RESUME 0x800102f
#define TAG_STREAM_SPR 0xC0000013

#define CONVX(x) #x
#define CONV_TO_STRING(x) CONVX(x)

#define ADD_MODULE(x, y) \
                 ({ \
                   module_info_t *add_mod = NULL;\
                   add_mod = calloc(1, sizeof(module_info_t));\
                   if (add_mod != NULL) {\
                       *add_mod = x;\
                       if (y != NULL) add_mod->dev_obj = y; \
                       list_add_tail(&graph_obj->tagged_mod_list, &add_mod->list);\
                   } \
                   add_mod;\
                 })


struct graph_obj {
    pthread_mutex_t lock;
    pthread_mutex_t gph_open_thread_lock;
    pthread_t gph_open_thread;
    pthread_cond_t gph_opened;
    bool gph_open_thread_created;
    graph_state_t state;
    gsl_handle_t graph_handle;
    struct listnode tagged_mod_list;
    struct gsl_cmd_configure_read_write_params buf_config;
    event_cb cb;
    void *client_data;
    struct session_obj *sess_obj;
    uint32_t spr_miid;
};

static int get_acdb_files_from_directory(const char* acdb_files_path,
                                         struct gsl_acdb_data_files *data_files)
{
    int result = 0;
    int ret = 0;
    int i = 0;
    DIR *dir_fp = NULL;
    struct dirent *dentry;

    dir_fp = opendir(acdb_files_path);
    if (dir_fp == NULL) {
        AGM_LOGE("cannot open acdb path %s\n", acdb_files_path);
        ret = EINVAL;
        goto done;
    }

    /* search directory for .acdb files */
    while ((dentry = readdir(dir_fp)) != NULL) {
        if ((strstr(dentry->d_name, ".acdb") != NULL)) {
           if (data_files->num_files >= GSL_MAX_NUM_OF_ACDB_FILES) {
               AGM_LOGE("Reached max num of files, %d!\n", i);
               break;
           }
           result = snprintf(
                    data_files->acdbFiles[i].fileName,
                    sizeof(data_files->acdbFiles[i].fileName),
                    "%s/%s", acdb_files_path, dentry->d_name);
           if ((result < 0) ||
               (result >= (int)sizeof(data_files->acdbFiles[i].fileName))) {
               AGM_LOGE("snprintf failed: %s/%s, err %d\n",
                              acdb_files_path,
                              data_files->acdbFiles[i].fileName,
                              result);
               ret = -EINVAL;
               break;
           }
           data_files->acdbFiles[i].fileNameLen =
                                    (uint32_t)strlen(data_files->acdbFiles[i].fileName);
           AGM_LOGI("Load file: %s\n", data_files->acdbFiles[i].fileName);
           i++;
	}
    }

    if (i == 0)
        AGM_LOGE("No .acdb files found in %s!\n", acdb_files_path);

    data_files->num_files = i;

    closedir(dir_fp);
done:
    return ret;
}

static void get_default_channel_map(uint8_t *channel_map, int channels)
{
    if (channels == 1)  {
        channel_map[0] = PCM_CHANNEL_C;
    } else if (channels == 2) {
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
    } else if (channels == 3) {
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
        channel_map[2] = PCM_CHANNEL_C;
    } else if (channels == 4) {
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
        channel_map[2] = PCM_CHANNEL_LB;
    	channel_map[3] = PCM_CHANNEL_RB;
    } else if (channels == 5) {
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
        channel_map[2] = PCM_CHANNEL_C;
        channel_map[3] = PCM_CHANNEL_LB;
        channel_map[4] = PCM_CHANNEL_RB;
    } else if (channels == 6) {
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
        channel_map[2] = PCM_CHANNEL_C;
        channel_map[3] = PCM_CHANNEL_LFE;
        channel_map[4] = PCM_CHANNEL_LB;
        channel_map[5] = PCM_CHANNEL_RB;
    } else if (channels == 7) {
        /*
         * Configured for 5.1 channel mapping + 1 channel for debug
         * Can be customized based on DSP.
         */
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
        channel_map[2] = PCM_CHANNEL_C;
        channel_map[3] = PCM_CHANNEL_LFE;
        channel_map[4] = PCM_CHANNEL_LB;
        channel_map[5] = PCM_CHANNEL_RB;
        channel_map[6] = PCM_CHANNEL_CS;
    } else if (channels == 8) {
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
        channel_map[2] = PCM_CHANNEL_C;
        channel_map[3] = PCM_CHANNEL_LFE;
        channel_map[4] = PCM_CHANNEL_LB;
        channel_map[5] = PCM_CHANNEL_RB;
        channel_map[6] = PCM_CHANNEL_LS;
        channel_map[7] = PCM_CHANNEL_RS;
    }
}

static bool is_format_pcm(enum agm_media_format fmt_id)
{
    if (fmt_id >= AGM_FORMAT_PCM_S8 && fmt_id <= AGM_FORMAT_PCM_S32_LE)
        return true;

    return false;
}

static int get_pcm_bit_width(enum agm_media_format fmt_id)
{
    int bit_width = 16;

    switch (fmt_id) {
    case AGM_FORMAT_PCM_S24_3LE:
    case AGM_FORMAT_PCM_S24_LE:
         bit_width = 24;
         break;
    case AGM_FORMAT_PCM_S32_LE:
         bit_width = 32;
         break;
    case AGM_FORMAT_PCM_S16_LE:
    default:
         break;
    }

    return bit_width;
}

static int get_media_bit_width(struct session_obj *sess_obj)
{
    int bit_width = 16;

    if (is_format_pcm(sess_obj->media_config.format))
        return get_pcm_bit_width(sess_obj->media_config.format);

    switch (sess_obj->media_config.format) {
    case AGM_FORMAT_FLAC:
        bit_width = sess_obj->stream_config.codec.flac_dec.sample_size;
        break;
    case AGM_FORMAT_ALAC:
        bit_width = sess_obj->stream_config.codec.alac_dec.bit_depth;
        break;
    case AGM_FORMAT_APE:
        bit_width = sess_obj->stream_config.codec.ape_dec.bit_width;
        break;
    case AGM_FORMAT_WMASTD:
        bit_width = sess_obj->stream_config.codec.wma_dec.bits_per_sample;
        break;
    case AGM_FORMAT_WMAPRO:
        bit_width = sess_obj->stream_config.codec.wmapro_dec.bits_per_sample;
        break;
    case AGM_FORMAT_MP3:
    case AGM_FORMAT_AAC:
    default:
         break;
    }

    return bit_width;
}

#define GET_BITS_PER_SAMPLE(format, bit_width) \
                           (format == AGM_FORMAT_PCM_S24_LE? 32 : bit_width)

#define GET_Q_FACTOR(format, bit_width)\
                     (format == AGM_FORMAT_PCM_S24_LE ? 27 : (bit_width - 1))

int configure_codec_dma_ep(struct module_info *mod,
                           struct graph_obj *graph_obj)
{
    int ret = 0;
    struct device_obj *dev_obj = mod->dev_obj;
    hw_ep_info_t hw_ep_info = dev_obj->hw_ep_info;
    struct gsl_key_vector tag_key_vect;
    struct apm_module_param_data_t *header;
    struct param_id_codec_dma_intf_cfg_t* codec_config;
    size_t payload_sz, ret_payload_sz = 0;
    uint8_t *payload = NULL;
    AGM_LOGD("entry mod tag %x miid %x mid %x", mod->tag, mod->miid, mod->mid);

    payload_sz = sizeof(struct apm_module_param_data_t) +
        sizeof(struct param_id_codec_dma_intf_cfg_t);

    if (payload_sz % 8 != 0)
        payload_sz = payload_sz + (8 - payload_sz % 8);

    ret_payload_sz = payload_sz;
    payload = (uint8_t*)calloc(1, (size_t)payload_sz);
    if (!payload) {
        AGM_LOGE("Not enough memory for payload");
        ret = -ENOMEM;
        goto done;
    }

    header = (struct apm_module_param_data_t*)payload;
    codec_config = (struct param_id_codec_dma_intf_cfg_t*)
                     (payload + sizeof(struct apm_module_param_data_t));

    /*
     * For Codec dma we need to configure the following tags
     * 1.Channels  - Channels are reused to derive the active channel mask
     */
    tag_key_vect.num_kvps = 1;
    tag_key_vect.kvp = calloc(tag_key_vect.num_kvps,
                                sizeof(struct gsl_key_value_pair));
    if (!tag_key_vect.kvp) {
        AGM_LOGE("Not enough memory for KVP");
        ret = -ENOMEM;
        goto free_payload;
    }

    tag_key_vect.kvp[0].key = CHANNELS;
    tag_key_vect.kvp[0].value = dev_obj->media_config.channels;

    ret = gsl_get_tagged_data((struct gsl_key_vector *)mod->gkv,
                               mod->tag, &tag_key_vect, (uint8_t *)payload,
                               &ret_payload_sz);

    if (ret != 0) {
        if (ret == CASA_ENEEDMORE)
           AGM_LOGE("payload buffer sz %d smaller than expected size %d",
                     payload_sz, ret_payload_sz);
  
        AGM_LOGE("get_tagged_data for mod tag:%x miid:%x failed with error %d",
                      mod->tag, mod->miid, ret);
        goto free_kvp;
    }

    AGM_LOGV("hdr mid %x pid %x er_cd %x param_sz %d",
             header->module_instance_id, header->param_id, header->error_code,
             header->param_size);

    codec_config->lpaif_type = hw_ep_info.lpaif_type;
    codec_config->intf_indx = hw_ep_info.intf_idx;

    AGM_LOGV("cdc_dma intf cfg lpaif %d indx %d ch_msk %x",
              codec_config->lpaif_type, codec_config->intf_indx,
              codec_config->active_channels_mask);

    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_sz);
    if (ret != 0) {
        AGM_LOGE("custom_config for module %d failed with error %d",
                      mod->tag, ret);
    }
free_kvp:
    free(tag_key_vect.kvp);
free_payload:
    free(payload);
done:
    AGM_LOGD("exit");
    return ret; 
}

int configure_i2s_ep(struct module_info *mod,
                           struct graph_obj *graph_obj)
{
    int ret = 0;
    struct device_obj *dev_obj = mod->dev_obj;
    hw_ep_info_t hw_ep_info = dev_obj->hw_ep_info;
    struct gsl_key_vector tag_key_vect;
    struct apm_module_param_data_t *header;
    struct  param_id_i2s_intf_cfg_t* i2s_config;
    size_t payload_sz, ret_payload_sz = 0;
    uint8_t *payload = NULL;
    AGM_LOGD("entry mod tag %x miid %x mid %x", mod->tag, mod->miid, mod->mid);

    payload_sz = sizeof(struct apm_module_param_data_t) +
        sizeof(struct  param_id_i2s_intf_cfg_t);

    if (payload_sz % 8 != 0)
        payload_sz = payload_sz + (8 - payload_sz % 8);

    ret_payload_sz = payload_sz;
    payload = (uint8_t*)calloc(1, (size_t)payload_sz);
    if (!payload) {
        AGM_LOGE("Not enough memory for payload");
        ret = -ENOMEM;
        goto done;
    }

    header = (struct apm_module_param_data_t*)payload;
    i2s_config = (struct  param_id_i2s_intf_cfg_t*)
                     (payload + sizeof(struct apm_module_param_data_t));

    /*
     * For Codec dma we need to configure the following tags
     * 1.Channels  - Channels are reused to derive the active channel mask
     */
    tag_key_vect.num_kvps = 1;
    tag_key_vect.kvp = calloc(tag_key_vect.num_kvps,
                                sizeof(struct gsl_key_value_pair));

    if (!tag_key_vect.kvp) {
        AGM_LOGE("Not enough memory for KVP");
        ret = -ENOMEM;
        goto free_payload;
    }

    tag_key_vect.kvp[0].key = CHANNELS;
    tag_key_vect.kvp[0].value = dev_obj->media_config.channels;

    ret = gsl_get_tagged_data((struct gsl_key_vector *)mod->gkv,
                               mod->tag, &tag_key_vect, (uint8_t *)payload,
                               &ret_payload_sz);

    if (ret != 0) {
        if (ret == CASA_ENEEDMORE)
           AGM_LOGE("payload buffer sz %d smaller than expected size %d",
                     payload_sz, ret_payload_sz);

        AGM_LOGE("get_tagged_data for module %d failed with error %d",
                      mod->tag, ret);
        goto free_kvp;
    }

    AGM_LOGV("hdr mid %x pid %x er_cd %x param_sz %d",
             header->module_instance_id, header->param_id, header->error_code,
             header->param_size);

    i2s_config->lpaif_type = hw_ep_info.lpaif_type;
    i2s_config->intf_idx = hw_ep_info.intf_idx;

    AGM_LOGV("i2s intf cfg lpaif %d indx %d sd_ln_idx %x ws_src %d",
              i2s_config->lpaif_type, i2s_config->intf_idx,
              i2s_config->sd_line_idx, i2s_config->ws_src);

    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_sz);
    if (ret != 0) {
        AGM_LOGE("custom_config for module %d failed with error %d",
                      mod->tag, ret);
    }
free_kvp:
    free(tag_key_vect.kvp);
free_payload:
    free(payload);
done:
    AGM_LOGD("exit");
    return ret; 
}

int configure_tdm_ep(struct module_info *mod,
                           struct graph_obj *graph_obj)
{
    int ret = 0;
    struct device_obj *dev_obj = mod->dev_obj;
    hw_ep_info_t hw_ep_info = dev_obj->hw_ep_info;
    struct gsl_key_vector tag_key_vect;
    struct apm_module_param_data_t *header;
    struct param_id_tdm_intf_cfg_t* tdm_config;
    size_t payload_sz, ret_payload_sz = 0;
    uint8_t *payload = NULL;
    AGM_LOGD("entry mod tag %x miid %x mid %x", mod->tag, mod->miid, mod->mid);

    payload_sz = sizeof(struct apm_module_param_data_t) +
        sizeof(struct param_id_tdm_intf_cfg_t);

    if (payload_sz % 8 != 0)
        payload_sz = payload_sz + (8 - payload_sz % 8);

    ret_payload_sz = payload_sz;
    payload = (uint8_t*)calloc(1, (size_t)payload_sz);
    if (!payload) {
        AGM_LOGE("Not enough memory for payload");
        ret = -ENOMEM;
        goto done;
    }

    header = (struct apm_module_param_data_t*)payload;
    tdm_config = (struct  param_id_tdm_intf_cfg_t*)
                     (payload + sizeof(struct apm_module_param_data_t));

    /*
     * For Codec dma we need to configure the following tags
     * 1.Channels  - Channels are reused to derive the active channel mask
     */
    tag_key_vect.num_kvps = 1;
    tag_key_vect.kvp = calloc(tag_key_vect.num_kvps,
                                sizeof(struct gsl_key_value_pair));

    if (!tag_key_vect.kvp) {
        AGM_LOGE("Not enough memory for KVP");
        ret = -ENOMEM;
        goto free_payload;
    }

    tag_key_vect.kvp[0].key = CHANNELS;
    tag_key_vect.kvp[0].value = dev_obj->media_config.channels;

    ret = gsl_get_tagged_data((struct gsl_key_vector *)mod->gkv,
                               mod->tag, &tag_key_vect, (uint8_t *)payload,
                               &ret_payload_sz);

    if (ret != 0) {

        if (ret == CASA_ENEEDMORE)
           AGM_LOGE("payload buffer sz %d smaller than expected size %d",
                     payload_sz, ret_payload_sz);

        AGM_LOGE("get_tagged_data for module %d failed with error %d",
                      mod->tag, ret);
        goto free_kvp;
    }

    tdm_config->lpaif_type = hw_ep_info.lpaif_type;
    tdm_config->intf_idx = hw_ep_info.intf_idx;

    AGM_LOGV("tdm intf cfg lpaif %d idx %d sync_src %d ctrl_dt_ot_enb %d",
             tdm_config->lpaif_type, tdm_config->intf_idx, tdm_config->sync_src,
             tdm_config->ctrl_data_out_enable);
    AGM_LOGV("slt_msk %x nslts_per_frm %x slt_wdth %d sync_mode %d",
             tdm_config->slot_mask, tdm_config->nslots_per_frame,
             tdm_config->slot_width, tdm_config->sync_mode);
    AGM_LOGV("inv_sync_pulse %d sync_data_delay %d",
             tdm_config->ctrl_invert_sync_pulse, tdm_config->ctrl_sync_data_delay);

    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_sz);
    if (ret != 0) {
        AGM_LOGE("custom_config for module %d failed with error %d",
                      mod->tag, ret);
    }
free_kvp:
    free(tag_key_vect.kvp);
free_payload:
    free(payload);
done:
    AGM_LOGD("exit");
    return ret; 
}

int configure_aux_pcm_ep(struct module_info *mod,
                           struct graph_obj *graph_obj)
{
    int ret = 0;
    struct device_obj *dev_obj = mod->dev_obj;
    hw_ep_info_t hw_ep_info = dev_obj->hw_ep_info;
    struct gsl_key_vector tag_key_vect;
    struct apm_module_param_data_t *header;
    struct param_id_tdm_intf_cfg_t* tdm_config;
    struct param_id_hw_pcm_intf_cfg_t* aux_pcm_cfg;
    size_t payload_sz ,ret_payload_sz = 0;
    uint8_t *payload = NULL;
    AGM_LOGD("entry mod tag %x miid %x mid %x", mod->tag, mod->miid, mod->mid);

    payload_sz = sizeof(struct apm_module_param_data_t) +
        sizeof(struct param_id_hw_pcm_intf_cfg_t);

    if (payload_sz % 8 != 0)
        payload_sz = payload_sz + (8 - payload_sz % 8);

    ret_payload_sz = payload_sz;
    payload = (uint8_t*)calloc(1, (size_t)payload_sz);
    if (!payload) {
        AGM_LOGE("Not enough memory for payload");
        ret = -ENOMEM;
        goto done;
    }

    header = (struct apm_module_param_data_t*)payload;
    aux_pcm_cfg = (struct  param_id_hw_pcm_intf_cfg_t*)
                     (payload + sizeof(struct apm_module_param_data_t));

    /*
     * For Codec dma we need to configure the following tags
     * 1.Channels  - Channels are reused to derive the active channel mask
     */
    tag_key_vect.num_kvps = 1;
    tag_key_vect.kvp = calloc(tag_key_vect.num_kvps,
                                sizeof(struct gsl_key_value_pair));

    if (!tag_key_vect.kvp) {
        AGM_LOGE("Not enough memory for KVP");
        ret = -ENOMEM;
        goto free_payload;
    }

    tag_key_vect.kvp[0].key = CHANNELS;
    tag_key_vect.kvp[0].value = dev_obj->media_config.channels;

    ret = gsl_get_tagged_data((struct gsl_key_vector *)mod->gkv,
                               mod->tag, &tag_key_vect, (uint8_t *)payload,
                               &ret_payload_sz);

    if (ret != 0) {
       if (ret == CASA_ENEEDMORE)
           AGM_LOGE("payload buffer sz %d smaller than expected size %d",
                     payload_sz, ret_payload_sz);

        AGM_LOGE("get_tagged_data for module %d failed with error %d",
                      mod->tag, ret);
        goto free_kvp;
    }

    aux_pcm_cfg->lpaif_type = hw_ep_info.lpaif_type;
    aux_pcm_cfg->intf_idx = hw_ep_info.intf_idx;

    AGM_LOGV("aux intf cfg lpaif %d idx %d sync_src %d ctrl_dt_ot_enb %d",
             aux_pcm_cfg->lpaif_type, aux_pcm_cfg->intf_idx, aux_pcm_cfg->sync_src,
             aux_pcm_cfg->ctrl_data_out_enable);
    AGM_LOGV("slt_msk %x frm_setting %x aux_mode %d",
             aux_pcm_cfg->slot_mask, aux_pcm_cfg->frame_setting,
             aux_pcm_cfg->aux_mode);

    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_sz);
    if (ret != 0) {
        AGM_LOGE("custom_config for module %d failed with error %d",
                      mod->tag, ret);
    }
free_kvp:
    free(tag_key_vect.kvp);
free_payload:
    free(payload);
done:
    AGM_LOGD("exit");
    return ret; 
}

int configure_hw_ep_media_config(struct module_info *mod,
                                struct graph_obj *graph_obj)
{
    int ret = 0;
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    struct device_obj *dev_obj = mod->dev_obj;
    struct apm_module_param_data_t* header;
    struct param_id_hw_ep_mf_t* hw_ep_media_conf;
    struct agm_media_config media_config = dev_obj->media_config;

    AGM_LOGD("entry mod tag %x miid %x mid %x",mod->tag, mod->miid, mod->mid);
    payload_size = sizeof(struct apm_module_param_data_t) +
                   sizeof(struct param_id_hw_ep_mf_t);

    /*ensure that the payloadszie is byte multiple atleast*/
    if (payload_size % 8 != 0)
        payload_size = payload_size + (8 - payload_size % 8);

    payload = calloc(1, (size_t)payload_size);
    if (!payload) {
        AGM_LOGE("No memory to allocate for payload");
        ret = -ENOMEM;
        goto done;
    }

    header = (struct apm_module_param_data_t*)payload;
    hw_ep_media_conf = (struct param_id_hw_ep_mf_t*)
                         (payload + sizeof(struct apm_module_param_data_t));
    
    header->module_instance_id = mod->miid;
    header->param_id = PARAM_ID_HW_EP_MF_CFG;
    header->error_code = 0x0;
    header->param_size = (uint32_t)payload_size;

    hw_ep_media_conf->sample_rate = media_config.rate;
    hw_ep_media_conf->bit_width = get_pcm_bit_width(media_config.format);

    hw_ep_media_conf->num_channels = media_config.channels;
    /*
     *TODO:Expose a parameter to client to update this as, this will change
     *once we move to supporting compress data through hw ep
     *
     */
    hw_ep_media_conf->data_format = DATA_FORMAT_FIXED_POINT;

    AGM_LOGV("rate %d bw %d ch %d", media_config.rate,
                    hw_ep_media_conf->bit_width, media_config.channels);

    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_size);
    if (ret != 0) {
        AGM_LOGE("custom_config command for module %d failed with error %d",
                      mod->tag, ret);
    }
    free(payload);
done:
    AGM_LOGD("exit");
    return ret;
}

int configure_hw_ep(struct module_info *mod,
                    struct graph_obj *graph_obj)
{
    int ret = 0;
    struct device_obj *dev_obj = mod->dev_obj;

    ret = configure_hw_ep_media_config(mod, graph_obj);
    if (ret) {
        AGM_LOGE("hw_ep_media_config failed %d", ret);
        return ret;
    }
    switch (dev_obj->hw_ep_info.intf) {
    case CODEC_DMA:
         ret = configure_codec_dma_ep(mod, graph_obj);
         break;
    case MI2S:
         ret = configure_i2s_ep(mod, graph_obj);
         break;
    case AUXPCM:
         ret = configure_aux_pcm_ep(mod, graph_obj);
         break;
    case TDM:
         ret = configure_tdm_ep(mod, graph_obj);
         break;
    default:
         AGM_LOGE("hw intf %d not enabled yet", dev_obj->hw_ep_info.intf);
         break;
    }
    return ret;
}

/**
 *PCM DECODER/ENCODER and PCM CONVERTER are configured with the
 *same PCM_FORMAT_CFG hence reuse the implementation
*/
int configure_output_media_format(struct module_info *mod,
                               struct graph_obj *graph_obj)
{
    struct session_obj *sess_obj = graph_obj->sess_obj;
    struct media_format_t *media_fmt_hdr;
    struct payload_pcm_output_format_cfg_t *pcm_output_fmt_payload;
    struct apm_module_param_data_t *header;
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    uint8_t *channel_map;
    int ret = 0;
    int num_channels = MONO;

    AGM_LOGD("entry mod tag %x miid %x mid %x",mod->tag, mod->miid, mod->mid);
    num_channels = sess_obj->media_config.channels;

    payload_size = sizeof(struct apm_module_param_data_t) +
                   sizeof(struct media_format_t) +
                   sizeof(struct payload_pcm_output_format_cfg_t) +
                   sizeof(uint8_t)*num_channels;

    /*ensure that the payloadszie is byte multiple atleast*/
    if (payload_size % 8 != 0)
        payload_size = payload_size + (8 - payload_size % 8);

    payload = malloc((size_t)payload_size);
    if (!payload) {
        AGM_LOGE("Not enough memory for payload");
        ret = -ENOMEM;
        return ret;
    }


    header = (struct apm_module_param_data_t*)payload;

    media_fmt_hdr = (struct media_format_t*)(payload +
                         sizeof(struct apm_module_param_data_t));
    pcm_output_fmt_payload = (struct payload_pcm_output_format_cfg_t*)(payload +
                             sizeof(struct apm_module_param_data_t) +
                             sizeof(struct media_format_t));

    channel_map = (uint8_t*)(payload + sizeof(struct apm_module_param_data_t) +
                                sizeof(struct media_format_t) +
                                sizeof(struct payload_pcm_output_format_cfg_t));

    header->module_instance_id = mod->miid;
    header->param_id = PARAM_ID_PCM_OUTPUT_FORMAT_CFG;
    header->error_code = 0x0;
    header->param_size = (uint32_t)payload_size;

    media_fmt_hdr->data_format = DATA_FORMAT_FIXED_POINT;
    media_fmt_hdr->fmt_id = MEDIA_FMT_ID_PCM;
    media_fmt_hdr->payload_size =
                      (uint32_t)(sizeof(payload_pcm_output_format_cfg_t) +
                                    sizeof(uint8_t) * num_channels);

    pcm_output_fmt_payload->endianness = PCM_LITTLE_ENDIAN;
    pcm_output_fmt_payload->bit_width = get_media_bit_width(sess_obj);
    /**
     *alignment field is referred to only in case where bit width is 
     *24 and bits per sample is 32, tiny alsa only supports 24 bit
     *in 32 word size in LSB aligned mode(AGM_FORMAT_PCM_S24_LE).
     *Hence we hardcode this to PCM_LSB_ALIGNED;
     */
    pcm_output_fmt_payload->alignment = PCM_LSB_ALIGNED;
    pcm_output_fmt_payload->num_channels = num_channels;
    pcm_output_fmt_payload->bits_per_sample =
                             GET_BITS_PER_SAMPLE(sess_obj->media_config.format,
                                                 pcm_output_fmt_payload->bit_width);

    pcm_output_fmt_payload->q_factor = GET_Q_FACTOR(sess_obj->media_config.format,
                                                pcm_output_fmt_payload->bit_width);

    if (sess_obj->stream_config.dir == RX)
        pcm_output_fmt_payload->interleaved = PCM_DEINTERLEAVED_UNPACKED;
    else
        pcm_output_fmt_payload->interleaved = PCM_INTERLEAVED;

    /**
     *#TODO:As of now channel_map is not part of media_config
     *ADD channel map part as part of the session/device media config
     *structure and use that channel map if set by client otherwise
     * use the default channel map
     */
    get_default_channel_map(channel_map, num_channels);
 
    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_size);
    if (ret != 0) {
        AGM_LOGE("custom_config command for module %d failed with error %d",
                      mod->tag, ret);
    }
    free(payload);
    AGM_LOGD("exit");
    return ret;
}

int get_media_fmt_id_and_size(enum agm_media_format fmt_id,
                              size_t *payload_size, size_t *real_fmt_id)
{
    int ret = 0;
    size_t format_size = 0;

    if (is_format_pcm(fmt_id)) {
        format_size = sizeof(struct payload_media_fmt_pcm_t);;
        *real_fmt_id = MEDIA_FMT_PCM;
        goto pl_size;
    }

    switch (fmt_id) {
    case AGM_FORMAT_MP3:
        format_size = 0;
        *real_fmt_id = MEDIA_FMT_MP3;
        break;
    case AGM_FORMAT_AAC:
        format_size = sizeof(struct payload_media_fmt_aac_t);
        *real_fmt_id = MEDIA_FMT_AAC;
        break;
    case AGM_FORMAT_FLAC:
        format_size = sizeof(struct payload_media_fmt_flac_t);
        *real_fmt_id = MEDIA_FMT_FLAC;
        break;
    case AGM_FORMAT_ALAC:
        format_size = sizeof(struct payload_media_fmt_alac_t);
        *real_fmt_id = MEDIA_FMT_ALAC;
        break;
    case AGM_FORMAT_APE:
        format_size = sizeof(struct payload_media_fmt_ape_t);
        *real_fmt_id = MEDIA_FMT_APE;
        break;
    case AGM_FORMAT_WMASTD:
        format_size = sizeof(struct payload_media_fmt_wmastd_t);
        *real_fmt_id = MEDIA_FMT_WMASTD;
        break;
    case AGM_FORMAT_WMAPRO:
        format_size = sizeof(struct payload_media_fmt_wmapro_t);
        *real_fmt_id = MEDIA_FMT_WMAPRO;
        break;
    case AGM_FORMAT_VORBIS:
        format_size = 0;
        *real_fmt_id = MEDIA_FMT_VORBIS;
        break;
    default:
        ret = -EINVAL;
        break;
    }

pl_size:
    *payload_size = sizeof(struct apm_module_param_data_t) +
                   sizeof(struct media_format_t) + format_size;

    return ret;
}


int set_media_format_payload(enum agm_media_format fmt_id,
                       struct media_format_t *media_fmt_hdr,
                       struct session_obj *sess_obj)
{
    int ret = 0;
    size_t fmt_size = 0;

    switch (fmt_id) {
    case AGM_FORMAT_MP3:
    {
        media_fmt_hdr->data_format = DATA_FORMAT_RAW_COMPRESSED ;
        media_fmt_hdr->fmt_id = MEDIA_FMT_ID_MP3;
        media_fmt_hdr->payload_size = 0;
        break;
    }
    case AGM_FORMAT_AAC:
    {
        struct payload_media_fmt_aac_t *fmt_pl;
        fmt_size = sizeof(struct payload_media_fmt_aac_t);
        media_fmt_hdr->data_format = DATA_FORMAT_RAW_COMPRESSED ;
        media_fmt_hdr->fmt_id = MEDIA_FMT_ID_AAC;
        media_fmt_hdr->payload_size = fmt_size;

        fmt_pl = (struct payload_media_fmt_aac_t*)media_fmt_hdr->payload;
        memcpy(fmt_pl, &sess_obj->stream_config.codec.aac_dec,
               fmt_size);
        AGM_LOGD("AAC payload: fmt:%d, Obj_type:%d, ch:%d, SR:%d",
                 fmt_pl->aac_fmt_flag, fmt_pl->audio_obj_type,
                 fmt_pl->num_channels, fmt_pl->sample_rate);
        break;
    }
    case AGM_FORMAT_FLAC:
    {
        struct payload_media_fmt_flac_t *fmt_pl;
        fmt_size = sizeof(struct payload_media_fmt_flac_t);
        media_fmt_hdr->data_format = DATA_FORMAT_RAW_COMPRESSED ;
        media_fmt_hdr->fmt_id = MEDIA_FMT_ID_FLAC;
        media_fmt_hdr->payload_size = fmt_size;

        fmt_pl = (struct payload_media_fmt_flac_t*)media_fmt_hdr->payload;
        memcpy(fmt_pl, &sess_obj->stream_config.codec.flac_dec,
               fmt_size);
        AGM_LOGD("FLAC payload: ch:%d, sample_size:%d, SR:%d",
                 fmt_pl->num_channels, fmt_pl->sample_size, fmt_pl->sample_rate);
        break;
    }
    case AGM_FORMAT_ALAC:
    {
        struct payload_media_fmt_alac_t *fmt_pl;
        fmt_size = sizeof(struct payload_media_fmt_alac_t);
        media_fmt_hdr->data_format = DATA_FORMAT_RAW_COMPRESSED ;
        media_fmt_hdr->fmt_id = MEDIA_FMT_ID_ALAC;
        media_fmt_hdr->payload_size = fmt_size;

        fmt_pl = (struct payload_media_fmt_alac_t*)media_fmt_hdr->payload;
        memcpy(fmt_pl, &sess_obj->stream_config.codec.alac_dec,
               fmt_size);
        AGM_LOGD("ALAC payload: bit_depth:%d, ch:%d, SR:%d",
                 fmt_pl->bit_depth, fmt_pl->num_channels,
                 fmt_pl->sample_rate);
        break;
    }
    case AGM_FORMAT_APE:
    {
        struct payload_media_fmt_ape_t *fmt_pl;
        fmt_size = sizeof(struct payload_media_fmt_ape_t);
        media_fmt_hdr->data_format = DATA_FORMAT_RAW_COMPRESSED ;
        media_fmt_hdr->fmt_id = MEDIA_FMT_ID_ALAC;
        media_fmt_hdr->payload_size = fmt_size;

        fmt_pl = (struct payload_media_fmt_ape_t*)media_fmt_hdr->payload;
        memcpy(fmt_pl, &sess_obj->stream_config.codec.ape_dec,
               fmt_size);
        AGM_LOGD("APE payload: bit_width:%d, ch:%d, SR:%d",
                 fmt_pl->bit_width, fmt_pl->num_channels,
                 fmt_pl->sample_rate);
        break;
    }
    case AGM_FORMAT_WMASTD:
    {
        struct payload_media_fmt_wmastd_t *fmt_pl;
        fmt_size = sizeof(struct payload_media_fmt_wmastd_t);
        media_fmt_hdr->data_format = DATA_FORMAT_RAW_COMPRESSED ;
        media_fmt_hdr->fmt_id = MEDIA_FMT_ID_WMASTD;
        media_fmt_hdr->payload_size = fmt_size;

        fmt_pl = (struct payload_media_fmt_wmastd_t*)media_fmt_hdr->payload;
        memcpy(fmt_pl, &sess_obj->stream_config.codec.wma_dec,
               fmt_size);
        AGM_LOGD("WMA payload: fmt:%d, ch:%d, SR:%d, bit_width:%d",
                  fmt_pl->fmt_tag, fmt_pl->num_channels,
                 fmt_pl->sample_rate, fmt_pl->bits_per_sample);
        break;
    }
    case AGM_FORMAT_WMAPRO:
    {
        struct payload_media_fmt_wmapro_t *fmt_pl;
        fmt_size = sizeof(struct payload_media_fmt_wmapro_t);
        media_fmt_hdr->data_format = DATA_FORMAT_RAW_COMPRESSED ;
        media_fmt_hdr->fmt_id = MEDIA_FMT_ID_WMAPRO;
        media_fmt_hdr->payload_size = fmt_size;

        fmt_pl = (struct payload_media_fmt_wmapro_t*)media_fmt_hdr->payload;
        memcpy(fmt_pl, &sess_obj->stream_config.codec.wmapro_dec,
               fmt_size);
        AGM_LOGD("WMAPro payload: fmt:%d, ch:%d, SR:%d, bit_width:%d",
                  fmt_pl->fmt_tag, fmt_pl->num_channels,
                 fmt_pl->sample_rate, fmt_pl->bits_per_sample);
        break;
    }
    default:
        return -EINVAL;
    }

    return ret;
}

/**
 *Configure placeholder decoder
*/
int configure_placeholder_dec(struct module_info *mod,
                              struct graph_obj *graph_obj)
{
    int ret = 0;
    struct gsl_key_vector tkv;
    struct session_obj *sess_obj = graph_obj->sess_obj;
    int fmt_id = 0;
    size_t payload_size = 0, real_fmt_id = 0;

    AGM_LOGE("enter");
    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    /* 1. Configure placeholder decoder with Real ID */
    ret = get_media_fmt_id_and_size(sess_obj->media_config.format,
                                    &payload_size, &real_fmt_id);
    if (ret) {
        AGM_LOGD("module is not configured for format: %d\n",
                 sess_obj->media_config.format);
        /* If ret is non-zero then placeholder module would be
         * configured by client so return from here.
         */
        return 0;
    }

    tkv.num_kvps = 1;
    tkv.kvp = calloc(tkv.num_kvps, sizeof(struct gsl_key_value_pair));
    tkv.kvp->key = MEDIA_FMT_ID;
    tkv.kvp->value = real_fmt_id;

    AGM_LOGD("Placeholder mod TKV key:%0x value: %0x", tkv.kvp->key,
             tkv.kvp->value);
    ret = gsl_set_config(graph_obj->graph_handle, (struct gsl_key_vector *)mod->gkv,
                         TAG_STREAM_PLACEHOLDER_DECODER, &tkv);
    if (ret != 0) {
        AGM_LOGE("set_config command failed with error: %d", ret);
        return ret;
    }

    /* 2. Set output media format cfg for placeholder decoder */
    ret = configure_output_media_format(mod, graph_obj);
    if (ret != 0)
        AGM_LOGE("output_media_format cfg failed: %d", ret);

    return ret;
}

int configure_compress_shared_mem_ep(struct module_info *mod,
                            struct graph_obj *graph_obj)
{
    int ret = 0;
    struct session_obj *sess_obj = graph_obj->sess_obj;
    struct media_format_t *media_fmt_hdr;
    struct apm_module_param_data_t *header;
    uint8_t *payload = NULL;
    size_t payload_size = 0, real_fmt_id = 0;

    ret = get_media_fmt_id_and_size(sess_obj->media_config.format,
                                    &payload_size, &real_fmt_id);
    if (ret) {
        AGM_LOGD("module is not configured for format: %d\n",
                 sess_obj->media_config.format);
        /* If ret is non-zero then shared memory module would be
         * configured by client so return from here.
         */
        return 0;
    }

    /*ensure that the payloadsize is byte multiple atleast*/
    if (payload_size % 8 != 0)
        payload_size = payload_size + (8 - payload_size % 8);

    payload = malloc((size_t)payload_size);

    header = (struct apm_module_param_data_t*)payload;

    media_fmt_hdr = (struct media_format_t*)(payload +
                         sizeof(struct apm_module_param_data_t));

    header->module_instance_id = mod->miid;
    header->param_id = PARAM_ID_MEDIA_FORMAT;
    header->error_code = 0x0;
    header->param_size = payload_size;

    ret = set_media_format_payload(sess_obj->media_config.format,
                             media_fmt_hdr, sess_obj);
    if (ret) {
        AGM_LOGD("Shared mem EP is not configured for format: %d\n",
                 sess_obj->media_config.format);
        /* If ret is non-zero then shared memory module would be
         * configured by client so return from here.
         */
        return 0;
    }

    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_size);
    if (ret != 0) {
        AGM_LOGE("custom_config command for module %d failed with error %d",
                      mod->tag, ret);
    }

    return ret;
}

int configure_pcm_shared_mem_ep(struct module_info *mod,
                            struct graph_obj *graph_obj)
{
    int ret = 0;
    struct session_obj *sess_obj = graph_obj->sess_obj;
    struct media_format_t *media_fmt_hdr;
    struct apm_module_param_data_t *header;
    struct payload_media_fmt_pcm_t *media_fmt_payload;
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    int num_channels = MONO;
    uint8_t *channel_map;

    AGM_LOGD("entry mod tag %x miid %x mid %x",mod->tag, mod->miid, mod->mid);
    num_channels = sess_obj->media_config.channels;

    payload_size = sizeof(struct apm_module_param_data_t) +
                   sizeof(struct media_format_t) +
                   sizeof(struct payload_media_fmt_pcm_t) +
                   sizeof(uint8_t)*num_channels;

    /*ensure that the payloadszie is byte multiple atleast*/
    if (payload_size % 8 != 0)
        payload_size = payload_size + (8 - payload_size % 8);

    payload = malloc((size_t)payload_size);
    if (!payload) {
        AGM_LOGE("Not enough memory for payload");
        ret = -ENOMEM;
        return ret;
    }


    header = (struct apm_module_param_data_t*)payload;

    media_fmt_hdr = (struct media_format_t*)(payload +
                         sizeof(struct apm_module_param_data_t));
    media_fmt_payload = (struct payload_media_fmt_pcm_t*)(payload +
                             sizeof(struct apm_module_param_data_t) +
                             sizeof(struct media_format_t));

    channel_map = (uint8_t*)(payload + sizeof(struct apm_module_param_data_t) +
                                sizeof(struct media_format_t) +
                                sizeof(struct payload_media_fmt_pcm_t));

    header->module_instance_id = mod->miid;
    header->param_id = PARAM_ID_MEDIA_FORMAT;
    header->error_code = 0x0;
    header->param_size = (uint32_t)payload_size;

    media_fmt_hdr->data_format = DATA_FORMAT_FIXED_POINT;
    media_fmt_hdr->fmt_id = MEDIA_FMT_ID_PCM;
    media_fmt_hdr->payload_size = (uint32_t)(sizeof(payload_media_fmt_pcm_t) +
                                     sizeof(uint8_t) * num_channels);

    media_fmt_payload->endianness = PCM_LITTLE_ENDIAN;
    media_fmt_payload->bit_width = get_media_bit_width(sess_obj);
    media_fmt_payload->sample_rate = sess_obj->media_config.rate;
    /**
     *alignment field is referred to only in case where bit width is 
     *24 and bits per sample is 32, tiny alsa only supports 24 bit
     *in 32 word size in LSB aligned mode(AGM_FORMAT_PCM_S24_LE).
     *Hence we hardcode this to PCM_LSB_ALIGNED;
     */
    media_fmt_payload->alignment = PCM_LSB_ALIGNED;
    media_fmt_payload->num_channels = num_channels;
    media_fmt_payload->bits_per_sample =
                             GET_BITS_PER_SAMPLE(sess_obj->media_config.format,
                                                 media_fmt_payload->bit_width);

    media_fmt_payload->q_factor = GET_Q_FACTOR(sess_obj->media_config.format,
                                                media_fmt_payload->bit_width);
    /**
     *#TODO:As of now channel_map is not part of media_config
     *ADD channel map part as part of the session/device media config
     *structure and use that channel map if set by client otherwise
     * use the default channel map
     */
    get_default_channel_map(channel_map, num_channels);
 
    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_size);
    if (ret != 0) {
        AGM_LOGE("custom_config command for module %d failed with error %d",
                      mod->tag, ret);
    }
    free(payload);
    AGM_LOGD("exit");
    return ret;
}

int configure_shared_mem_ep(struct module_info *mod,
                            struct graph_obj *graph_obj)
{
    int ret = 0;
    struct session_obj *sess_obj = graph_obj->sess_obj;

    if (is_format_pcm(sess_obj->media_config.format))
        ret = configure_pcm_shared_mem_ep(mod, graph_obj);
    else
        ret = configure_compress_shared_mem_ep(mod, graph_obj);

    if (ret)
        return ret;

    return 0;
}

int configure_spr(struct module_info *spr_mod,
                            struct graph_obj *graph_obj)
{
    int ret = 0;
    struct session_obj *sess_obj = graph_obj->sess_obj;
    struct listnode *node = NULL;
    struct module_info *mod;
    struct apm_module_param_data_t *header;
    struct param_id_spr_delay_path_end_t *spr_hwep_delay;
    uint8_t *payload = NULL;
    size_t payload_size = 0;

    AGM_LOGD("SPR module IID %x", spr_mod->miid);
    graph_obj->spr_miid = spr_mod->miid;
    payload_size = sizeof(struct apm_module_param_data_t) +
                    sizeof(struct param_id_spr_delay_path_end_t);
    if (payload_size % 8 != 0)
        payload_size = payload_size + (8 - payload_size % 8);

    payload = calloc(1, (size_t)payload_size);
    if (!payload) {
        AGM_LOGE("No memory to allocate for payload");
        ret = -ENOMEM;
        goto done;
    }
    header = (struct apm_module_param_data_t*)payload;
    spr_hwep_delay = (struct param_id_spr_delay_path_end_t *)(payload
                          + sizeof(struct apm_module_param_data_t));
    header->module_instance_id = spr_mod->miid;
    header->param_id = PARAM_ID_SPR_DELAY_PATH_END;
    header->error_code = 0x0;
    header->param_size = payload_size;

    list_for_each(node, &graph_obj->tagged_mod_list) {
        mod = node_to_item(node, module_info_t, list);
        if (mod->tag == DEVICE_HW_ENDPOINT_RX) {
            AGM_LOGD("HW EP module IID %x", mod->miid);
            spr_hwep_delay->module_instance_id = mod->miid;
            ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_size);
            if (ret !=0)
                AGM_LOGE("graph_set_custom_config failed %d", ret);
        }
    }
done:
    return ret;
}

static module_info_t hw_ep_module[] = {
   {
       .module = MODULE_HW_EP_RX,
       .tag = DEVICE_HW_ENDPOINT_RX,
       .configure = configure_hw_ep,
   },
   {
       .module = MODULE_HW_EP_TX,
       .tag = DEVICE_HW_ENDPOINT_TX,
       .configure = configure_hw_ep,
   }
};

static module_info_t stream_module_list[] = {
 {
     .module = MODULE_PCM_ENCODER,
     .tag = STREAM_PCM_ENCODER,
     .configure = configure_output_media_format,
 },
 {
     .module = MODULE_PCM_DECODER,
     .tag = STREAM_PCM_DECODER,
     .configure = configure_output_media_format,
 },
 {
     .module = MODULE_PLACEHOLDER_DECODER,
     .tag = TAG_STREAM_PLACEHOLDER_DECODER,
     .configure = configure_placeholder_dec,
 },
 {
     .module = MODULE_PCM_CONVERTER,
     .tag = STREAM_PCM_CONVERTER,
     .configure = configure_output_media_format,
 },
 {
     .module = MODULE_SHARED_MEM,
     .tag = STREAM_INPUT_MEDIA_FORMAT,
     .configure = configure_shared_mem_ep,
 },
 {
     .module = MODULE_STREAM_PAUSE,
     .tag = TAG_PAUSE,
     .configure = NULL,
 },
 {
     .module = MODULE_STREAM_SPR,
     .tag = TAG_STREAM_SPR,
     .configure = configure_spr,
 },
 };

int configure_buffer_params(struct graph_obj *gph_obj,
                            struct session_obj *sess_obj)
{
    struct gsl_cmd_configure_read_write_params buf_config;
    int ret = 0;
    size_t size = 0;
    enum gsl_cmd_id cmd_id;
    enum agm_data_mode mode = sess_obj->stream_config.data_mode;

    AGM_LOGD("%s sess buf_sz %d num_bufs %d", sess_obj->stream_config.dir == RX?
                 "Playback":"Capture", sess_obj->buffer_config.size,
                  sess_obj->buffer_config.count);

    buf_config.buff_size = (uint32_t)sess_obj->buffer_config.size;
    buf_config.num_buffs = sess_obj->buffer_config.count;
    buf_config.start_threshold = sess_obj->stream_config.start_threshold;
    buf_config.stop_threshold = sess_obj->stream_config.stop_threshold;
    /**
     *TODO:expose a flag to chose between different data passing modes
     *BLOCKING/NON-BLOCKING/SHARED_MEM.
     */
    if (mode == AGM_DATA_BLOCKING)
        buf_config.attributes = GSL_DATA_MODE_BLOCKING;
    else if (mode == AGM_DATA_NON_BLOCKING)
        buf_config.attributes = GSL_DATA_MODE_NON_BLOCKING;
    else {
        AGM_LOGE("Unsupported buffer mode : %d, Default to Blocking", mode);
        buf_config.attributes = GSL_DATA_MODE_BLOCKING;
    }

    size = sizeof(struct gsl_cmd_configure_read_write_params);

    if (sess_obj->stream_config.dir == RX)
       cmd_id = GSL_CMD_CONFIGURE_WRITE_PARAMS;
    else
       cmd_id = GSL_CMD_CONFIGURE_READ_PARAMS;

    ret = gsl_ioctl(gph_obj->graph_handle, cmd_id, &buf_config, size);

    if (ret != 0) {
        AGM_LOGE("Buffer configuration failed error %d", ret);
    } else {
       gph_obj->buf_config  = buf_config;
    }
    AGM_LOGD("exit");
    return ret;
}

int graph_init()
{
    uint32_t ret = 0;
    struct gsl_acdb_data_files acdb_files;
    struct gsl_init_data init_data;

    /*Populate acdbfiles from the shared file path*/
    acdb_files.num_files = 0;

#ifdef ACDB_PATH
    ret = get_acdb_files_from_directory(CONV_TO_STRING(ACDB_PATH), &acdb_files);
    if (ret != 0)
       return ret;
#else
#  error "Define -DACDB_PATH="PATH" in the makefile to compile"
#endif

    init_data.acdb_files = &acdb_files;
    init_data.acdb_delta_file = NULL;
    init_data.acdb_addr = 0x0;
    init_data.max_num_ready_checks = 1;
    init_data.ready_check_interval_ms = 100;

    ret = gsl_init(&init_data);
    if (ret != 0) {
        AGM_LOGE("gsl_init failed error %d \n", ret);
        goto deinit_gsl;
    }
    return 0;

deinit_gsl:
    gsl_deinit();
    return ret;
}

int graph_deinit()
{

    gsl_deinit();
    return 0;
}

void gsl_callback_func(struct gsl_event_cb_params *event_params,
                       void *client_data)
{
     struct graph_obj *graph_obj = (struct graph_obj *) client_data;

     if (graph_obj == NULL) {
         AGM_LOGE("Invalid graph object");
         return;
     }
     if (event_params == NULL) {
         AGM_LOGE("event params NULL");
         return;
     }

     if (graph_obj->cb)
         graph_obj->cb((struct agm_event_cb_params *)event_params,
                        graph_obj->client_data);

     return;
}

int graph_get_tags_with_module_info(struct agm_key_vector_gsl *gkv,
				    void *payload, size_t *size)
{
    return gsl_get_tags_with_module_info((struct gsl_key_vector *) gkv,
                                         payload, size);
}

int graph_open(struct agm_meta_data_gsl *meta_data_kv,
               struct session_obj *ses_obj, struct device_obj *dev_obj,
               struct graph_obj **gph_obj)
{
    struct graph_obj *graph_obj = NULL;
    int ret = 0;
    struct listnode *temp_node,*node = NULL;
    size_t module_info_size;
    struct gsl_module_id_info *module_info;
    struct agm_key_vector_gsl *gkv;
    int i = 0;
    module_info_t *mod, *temp_mod = NULL;

    AGM_LOGD("entry");
    if (meta_data_kv == NULL || gph_obj == NULL) {
        AGM_LOGE("Invalid input\n");
        ret = -EINVAL;
        goto done;
    }

    graph_obj = calloc (1, sizeof(struct graph_obj));
    if (graph_obj == NULL) {
        AGM_LOGE("failed to allocate graph object");
        ret = -ENOMEM;
        goto done;
    }
    list_init(&graph_obj->tagged_mod_list);
    pthread_mutex_init(&graph_obj->lock, (const pthread_mutexattr_t *)NULL);
    /**
     *TODO
     *Once the gsl_get_tag_module_list api is availiable use that api
     *to get the list of tagged modules in the current graph and add
     *it to module_list so that we configure only those modules which
     *are present. For now we query for all modules and add only those
     *are present.
     */

    /**
     *TODO:In the current config parameters we dont have a
     *way to know if a hostless session (loopback)
     *is session loopback or device loopback
     *We assume now that if it is a hostless
     *session then it is device loopback always.
     *And hence we configure pcm decoder/encoder/convertor
     *only in case of a no hostless session.
     */
    if (ses_obj != NULL) {
        int count = sizeof(stream_module_list)/sizeof(struct module_info);
        for (i = 0; i < count; i++) {
             mod = &stream_module_list[i];
             ret = gsl_get_tagged_module_info((struct gsl_key_vector *)
                                               &meta_data_kv->gkv,
                                               mod->tag,
                                               &module_info, &module_info_size);
             if ((ret != 0) || (module_info == NULL)) {
                 AGM_LOGI("cannot get tagged module info for module %x",
                           mod->tag);
                 continue;
             }
             mod->miid = module_info->module_entry[0].module_iid;
             mod->mid = module_info->module_entry[0].module_id;
             AGM_LOGD("miid %x mid %x tag %x", mod->miid, mod->mid, mod->tag);
             ADD_MODULE(*mod, NULL); 
             if (module_info)
                free(module_info);
        }
        graph_obj->sess_obj = ses_obj;
    }

    if (dev_obj != NULL) {

        int count = sizeof(hw_ep_module)/sizeof(struct module_info);

        for (i = 0; i < count; i++) {
             mod = &hw_ep_module[i];
             ret = gsl_get_tagged_module_info((struct gsl_key_vector *)
                                               &meta_data_kv->gkv,
                                               mod->tag,
                                               &module_info, &module_info_size);
             if ((ret != 0) || (module_info == NULL)) {
                 AGM_LOGI("cannot get tagged module info for module %x",
                           mod->tag);
                 continue;
             }
             mod->miid = module_info->module_entry[0].module_iid;
             mod->mid = module_info->module_entry[0].module_id;
             /*store GKV which describes/contains this module*/
             gkv = calloc(1, sizeof(struct agm_key_vector_gsl));
             if (!gkv) {
                 AGM_LOGE("No memory to create merged metadata\n");
                 ret = -ENOMEM;
                 goto free_graph_obj;
             }

             gkv->num_kvs = meta_data_kv->gkv.num_kvs;
             gkv->kv = calloc(gkv->num_kvs, sizeof(struct agm_key_value));
             if (!gkv->kv) {
                 AGM_LOGE("No memory to create merged metadata gkv\n");
                 free(gkv);
                 ret = -ENOMEM;
                 goto free_graph_obj;
             }
             memcpy(gkv->kv, meta_data_kv->gkv.kv,
                    gkv->num_kvs * sizeof(struct agm_key_value));
             mod->gkv = gkv;
             AGM_LOGD("miid %x mid %x tag %x", mod->miid, mod->mid, mod->tag);
             ADD_MODULE(*mod, dev_obj);
             if (module_info)
                 free(module_info);
             gkv = NULL;
        }
    }

    ret = gsl_open((struct gsl_key_vector *)&meta_data_kv->gkv,
                   (struct gsl_key_vector *)&meta_data_kv->ckv,
                   &graph_obj->graph_handle);
    if (ret != 0) {
       AGM_LOGE("Failed to open the graph with error %d", ret);
       goto free_graph_obj;
    }

    ret = gsl_register_event_cb(graph_obj->graph_handle,
                                gsl_callback_func, graph_obj);
    if (ret != 0) {
        AGM_LOGE("failed to register callback");
        goto close_graph;
    }
    graph_obj->state = OPENED;
    *gph_obj = graph_obj;

    goto done;

close_graph:
    gsl_close(graph_obj->graph_handle);
free_graph_obj:
    /*free the list of modules associated with this graph_object*/
    list_for_each_safe(node, temp_node, &graph_obj->tagged_mod_list) {
        list_remove(node);
        temp_mod = node_to_item(node, module_info_t, list);
        if (temp_mod->gkv) {
            free(temp_mod->gkv->kv);
            free(temp_mod->gkv);
        }
        free(temp_mod);
    }
    pthread_mutex_destroy(&graph_obj->lock);
    free(graph_obj);
done:
    return ret;   
}

int graph_close(struct graph_obj *graph_obj)
{
    int ret = 0;
    struct listnode *temp_node,*node = NULL;
    module_info_t *temp_mod = NULL;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }
    pthread_mutex_lock(&graph_obj->lock); 
    AGM_LOGD("entry handle %x", graph_obj->graph_handle);
    if (graph_obj->state < OPENED) {
        AGM_LOGE("graph object not in opened state");
        pthread_mutex_unlock(&graph_obj->lock); 
        ret = -EINVAL;
    }

    ret = gsl_close(graph_obj->graph_handle);
    if (ret !=0) 
        AGM_LOGE("gsl close failed error %d", ret);
    AGM_LOGE("gsl graph_closed");
    /*free the list of modules associated with this graph_object*/
    list_for_each_safe(node, temp_node, &graph_obj->tagged_mod_list) {
        list_remove(node);
        temp_mod = node_to_item(node, module_info_t, list);
        if (temp_mod->gkv) {
            free(temp_mod->gkv->kv);
            free(temp_mod->gkv);
        }
        free(temp_mod);
    }
    pthread_mutex_unlock(&graph_obj->lock);
    pthread_mutex_destroy(&graph_obj->lock);
    free(graph_obj);
    AGM_LOGD("exit");
    return ret; 
}

int graph_prepare(struct graph_obj *graph_obj)
{
    int ret = 0;
    struct listnode *node = NULL;
    module_info_t *mod = NULL;
    struct session_obj *sess_obj = NULL;
    struct agm_session_config stream_config;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }
    sess_obj = graph_obj->sess_obj;

    if (sess_obj == NULL) {
        AGM_LOGE("invalid sess object");
        return -EINVAL;
    }
    stream_config = sess_obj->stream_config;

    AGM_LOGD("entry graph_handle %x", graph_obj->graph_handle);
    pthread_mutex_lock(&graph_obj->lock);
    /**
     *Iterate over mod list to configure each module
     *present in the graph. Also validate if the module list
     *matches the configuration passed by the client.
     */
    list_for_each(node, &graph_obj->tagged_mod_list) {
        mod = node_to_item(node, module_info_t, list);
        if (mod->is_configured)
            continue;
        if ((mod->tag == STREAM_INPUT_MEDIA_FORMAT) && stream_config.is_hostless) {
            AGM_LOGE("Shared mem mod present for a hostless session error out");
            ret = -EINVAL;
            goto done;
        }

        if (((mod->tag == STREAM_PCM_DECODER) &&
              (stream_config.dir == TX)) ||
            ((mod->tag == STREAM_PCM_ENCODER) &&
              (stream_config.dir == RX))) {
            AGM_LOGE("Session cfg (dir = %d) does not match session module %x",
                      stream_config.dir, mod->module);
             ret = -EINVAL;
             goto done;
        }

        if ((mod->dev_obj != NULL) && (mod->dev_obj->refcnt.start == 0)) {
            if (((mod->tag == DEVICE_HW_ENDPOINT_RX) &&
                (mod->dev_obj->hw_ep_info.dir == AUDIO_INPUT)) ||
               ((mod->tag == DEVICE_HW_ENDPOINT_TX) &&
                (mod->dev_obj->hw_ep_info.dir == AUDIO_OUTPUT))) {
               AGM_LOGE("device cfg (dir = %d) does not match dev module %x",
                         mod->dev_obj->hw_ep_info.dir, mod->module);
               ret = -EINVAL;
               goto done;
            }
        }
        if (mod->configure) {
            ret = mod->configure(mod, graph_obj);
            if (ret != 0)
                goto done;
            mod->is_configured = true;
        }
    }

    /*Configure buffers only if it is not a hostless session*/
    if (sess_obj != NULL && !stream_config.is_hostless) {
        ret = configure_buffer_params(graph_obj, sess_obj);
        if (ret != 0) {
            AGM_LOGE("buffer configuration failed \n");
            goto done;
        }
    }

    ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_PREPARE, NULL, 0);
    if (ret !=0) {
        AGM_LOGE("graph_prepare failed %d", ret);
    } else {
        graph_obj->state = PREPARED;
    }

done:
    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_start(struct graph_obj *graph_obj)
{
    int ret = 0;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %x", graph_obj->graph_handle);
    if (!(graph_obj->state & (PREPARED | STOPPED))) {
       AGM_LOGE("graph object is not in correct state, current state %d",
                    graph_obj->state);
       ret = -EINVAL;
       goto done;
    }
    ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_START, NULL, 0); 
    if (ret !=0) {
        AGM_LOGE("graph_start failed %d", ret);
    } else {
        graph_obj->state = STARTED;
    }

done:
    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_stop(struct graph_obj *graph_obj,
               struct agm_meta_data_gsl *meta_data)
{
    int ret = 0;
    struct gsl_cmd_properties gsl_cmd_prop = {0};

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %x", graph_obj->graph_handle);
    if (!(graph_obj->state & (STARTED))) {
       AGM_LOGE("graph object is not in correct state, current state %d",
                    graph_obj->state);
       ret = -EINVAL;
       goto done;
    }

    if (meta_data) {
        memcpy (&(gsl_cmd_prop.gkv), &(meta_data->gkv),
                                       sizeof(struct gsl_key_vector));
        gsl_cmd_prop.property_id = meta_data->sg_props.prop_id;
        gsl_cmd_prop.num_property_values = meta_data->sg_props.num_values;
        gsl_cmd_prop.property_values = meta_data->sg_props.values;

        ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_STOP,
                        &gsl_cmd_prop, sizeof(struct gsl_cmd_properties));
    } else {
        ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_STOP, NULL, 0);
    }
    if (ret !=0) {
        AGM_LOGE("graph stop failed %d", ret);
    } else {
        graph_obj->state = STOPPED;
    }

done:
    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_pause_resume(struct graph_obj *graph_obj, bool pause)
{
    int ret = 0;
    struct listnode *node = NULL;
    module_info_t *mod;
    struct apm_module_param_data_t *header;
    size_t payload_size = 0;
    uint8_t *payload = NULL;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    /* Pause module info is retrived and added to list in graph_open */
    list_for_each(node, &graph_obj->tagged_mod_list) {
        mod = node_to_item(node, module_info_t, list);
        if (mod->tag == TAG_PAUSE) {
            AGM_LOGD("Soft Pause module IID 0x%x, Pause: %d", mod->miid, pause);

            payload_size = sizeof(struct apm_module_param_data_t);
            if (payload_size % 8 != 0)
                payload_size = payload_size + (8 - payload_size % 8);

            payload = calloc(1, (size_t)payload_size);
            if (!payload) {
                AGM_LOGE("No memory to allocate for payload");
                ret = -ENOMEM;
                goto done;
            }

            header = (struct apm_module_param_data_t*)payload;
            header->module_instance_id = mod->miid;
            if (pause)
                header->param_id = PARAM_ID_SOFT_PAUSE_START;
            else
                header->param_id = PARAM_ID_SOFT_PAUSE_RESUME;

            header->error_code = 0x0;
            header->param_size = 0x0;

            pthread_mutex_lock(&graph_obj->lock);
            ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_size);
            if (ret !=0)
                AGM_LOGE("graph_set_custom_config failed %d", ret);
            pthread_mutex_unlock(&graph_obj->lock);
            free(payload);
            break;
        }
    }

done:
    return ret;
}


int graph_pause(struct graph_obj *graph_obj)
{
    return graph_pause_resume(graph_obj, true);
}

int graph_resume(struct graph_obj *graph_obj)
{
    return graph_pause_resume(graph_obj, false);
}

int graph_set_config(struct graph_obj *graph_obj, void *payload,
                     size_t payload_size)
{
    int ret = 0;
    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %x", graph_obj->graph_handle);
    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_size); 
    if (ret !=0)
        AGM_LOGE("%s: graph_set_config failed %d", __func__, ret);

    pthread_mutex_unlock(&graph_obj->lock);

    return ret;
}

int graph_set_config_with_tag(struct graph_obj *graph_obj,
                              struct agm_key_vector_gsl *gkv,
                              struct agm_tag_config_gsl *tag_config)
{
     int ret = 0;

     if (graph_obj == NULL) {
         AGM_LOGE("invalid graph object");
         return -EINVAL;
     }

     pthread_mutex_lock(&graph_obj->lock);
     ret = gsl_set_config(graph_obj->graph_handle, (struct gsl_key_vector *)&gkv,
                          tag_config->tag_id, (struct gsl_key_vector *)&tag_config->tkv);
     if (ret)
         AGM_LOGE("graph_set_config failed %d", ret);

     pthread_mutex_unlock(&graph_obj->lock);

     return ret;
}

int graph_set_cal(struct graph_obj *graph_obj,
                  struct agm_meta_data_gsl *metadata)
{
     int ret = 0;

     if (graph_obj == NULL) {
         AGM_LOGE("invalid graph object");
         return -EINVAL;
     }

     pthread_mutex_lock(&graph_obj->lock);
     ret = gsl_set_cal(graph_obj->graph_handle,
                       (struct gsl_key_vector *)&metadata->gkv,
                       (struct gsl_key_vector *)&metadata->ckv);
     if (ret)
         AGM_LOGE("graph_set_cal failed %d", ret);

     pthread_mutex_unlock(&graph_obj->lock);

     return ret;
}

int graph_write(struct graph_obj *graph_obj, void *buffer, size_t *size)
{
    int ret = 0;
    struct gsl_buff gsl_buff;
    uint32_t size_written = 0;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }
    pthread_mutex_lock(&graph_obj->lock);
    // TODO: update below check
   /* if (!(graph_obj->state & (PREPARED|STARTED))) {
        AGM_LOGE("Cannot add a graph in start state");
        ret = -EINVAL;
        goto done;
    }*/

    /*TODO: Update the write api to take timeStamps/other buffer meta data*/
    gsl_buff.timestamp = 0x0;
    /*TODO: get the FLAG info from client e.g. FLAG_EOS)*/
    gsl_buff.flags = 0;
    gsl_buff.size = *size;
    gsl_buff.addr = (uint8_t *)(buffer);
    ret = gsl_write(graph_obj->graph_handle,
                    SHMEM_ENDPOINT, &gsl_buff, &size_written);
    if (ret != 0) {
        AGM_LOGE("gsl_write for size %d failed with error %d", size, ret);
        goto done;
    }
    *size = size_written;
done:
    pthread_mutex_unlock(&graph_obj->lock);

    return ret;
}

int graph_read(struct graph_obj *graph_obj, void *buffer, size_t *size)
{
    int ret = 0;
    struct gsl_buff gsl_buff;
    int size_read = 0;
    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }
    pthread_mutex_lock(&graph_obj->lock);
    if (!(graph_obj->state & STARTED)) {
        AGM_LOGE("Cannot add a graph in start state");
        ret = -EINVAL;
        goto done;
    }

    /*TODO: Update the write api to take timeStamps/other buffer meta data*/
    gsl_buff.timestamp = 0x0;
    /*TODO: get the FLAG info from client e.g. FLAG_EOS)*/
    gsl_buff.flags = 0;
    gsl_buff.size = *size;
    gsl_buff.addr = (uint8_t *)(buffer);
    ret = gsl_read(graph_obj->graph_handle,
                    SHMEM_ENDPOINT, &gsl_buff, &size_read);
    if ((ret != 0) || (size_read == 0)) {
        AGM_LOGE("size_requested %d size_read %d error %d",
                      size, size_read, ret);
    }
    *size = size_read;
done:
    pthread_mutex_unlock(&graph_obj->lock);
    return ret;
}

int graph_add(struct graph_obj *graph_obj,
              struct agm_meta_data_gsl *meta_data_kv,
              struct device_obj *dev_obj)
{
    int ret = 0;
    struct session_obj *sess_obj;
    struct gsl_cmd_graph_select add_graph;
    module_info_t *mod = NULL;
    struct agm_key_vector_gsl *gkv;
    struct listnode *node = NULL;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %x", graph_obj->graph_handle);

    if (graph_obj->state < OPENED) {
        AGM_LOGE("Cannot add a graph in %d state", graph_obj->state);
        ret = -EINVAL;
        goto done;
    }
    /**
     *This api is used to add a new device leg ( is device object is
     *present in the argument or to add an already exisiting graph.
     *Hence we need not add any new session (stream) related modules
     */

    /*Add the new GKV to the current graph*/
    add_graph.graph_key_vector.num_kvps = meta_data_kv->gkv.num_kvs;
    add_graph.graph_key_vector.kvp = (struct gsl_key_value_pair *)
                                     meta_data_kv->gkv.kv;
    add_graph.cal_key_vect.num_kvps = meta_data_kv->ckv.num_kvs;
    add_graph.cal_key_vect.kvp = (struct gsl_key_value_pair *)
                                     meta_data_kv->ckv.kv;
    ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_ADD_GRAPH, &add_graph,
                    sizeof(struct gsl_cmd_graph_select));
    if (ret != 0) {
        AGM_LOGE("graph add failed with error %d", ret);
        goto done;
    }
    if (dev_obj != NULL) {
        module_info_t *add_module, *temp_mod = NULL;
        size_t module_info_size;
        struct gsl_module_id_info *module_info;
        bool mod_present = false;
        if (dev_obj->hw_ep_info.dir == AUDIO_OUTPUT)
            mod = &hw_ep_module[0];
        else
            mod = &hw_ep_module[1];

        ret = gsl_get_tagged_module_info((struct gsl_key_vector *)
                                           &meta_data_kv->gkv,
                                           mod->tag,
                                           &module_info, &module_info_size);
        if (ret != 0) {
            AGM_LOGE("cannot get tagged module info for module %x",
                          mod->tag);
            ret = -EINVAL;
            goto done;
        }
        mod->miid = module_info->module_entry[0].module_iid;
        mod->mid = module_info->module_entry[0].module_id;
        /**
         *Check if this is the same device object as was passed for graph open
         *or a new one.We do this by comparing the module_iid of the module
         *present in the graph object with the one returned from the above api.
         *if it is a new module we add it to the list and configure it.
         */
        list_for_each(node, &graph_obj->tagged_mod_list) {
            temp_mod = node_to_item(node, module_info_t, list);
            if (temp_mod->miid == mod->miid) {
                mod_present = true;
                break;
            }
        }
        if (!mod_present) {
            /**
             *This is a new device object, add this module to the list and
             */
            /*Make a local copy of gkv and use when we query gsl
            for tagged data*/
            gkv = calloc(1, sizeof(struct agm_key_vector_gsl));
            if (!gkv) {
                AGM_LOGE("No memory to allocate for gkv\n");
                ret = -ENOMEM;
                goto done;
            }
            gkv->num_kvs = meta_data_kv->gkv.num_kvs;
            gkv->kv = calloc(gkv->num_kvs, sizeof(struct agm_key_value));
            if (!gkv->kv) {
                AGM_LOGE("No memory to allocate for kv\n");
                free(gkv);
                ret = -ENOMEM;
                goto done;
            }
            memcpy(gkv->kv, meta_data_kv->gkv.kv,
                    gkv->num_kvs * sizeof(struct agm_key_value));
            mod->gkv = gkv;
            gkv = NULL;
            AGM_LOGD("Adding the new module tag %x mid %x miid %x", mod->tag, mod->mid, mod->miid);
            ADD_MODULE(*mod, dev_obj);
        }
    }
    /*configure the newly added modules*/
    list_for_each(node, &graph_obj->tagged_mod_list) {
        mod = node_to_item(node, module_info_t, list);
        /* Need to configure SPR module again for the new device */
        if (mod->is_configured && !(mod->tag == TAG_STREAM_SPR))
            continue;
        if (mod->configure) {
            ret = mod->configure(mod, graph_obj);
            if (ret != 0)
                goto done;
            mod->is_configured = true;
        }
    }
done:
    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_change(struct graph_obj *graph_obj,
                     struct agm_meta_data_gsl *meta_data_kv,
                     struct device_obj *dev_obj)
{
    int ret = 0;
    struct session_obj *sess_obj;
    struct gsl_cmd_graph_select change_graph;
    module_info_t *mod = NULL;
    struct agm_key_vector_gsl *gkv;
    struct listnode *node, *temp_node = NULL;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %x", graph_obj->graph_handle);
    if (graph_obj->state & STARTED) {
        AGM_LOGE("Cannot change graph in start state");
        ret = -EINVAL;
        goto done;
    }

    /**
     *GSL closes the old graph if CHANGE_GRAPH command is issued.
     *Hence reset is_configured to ensure that all the modules are 
     *configured once again.
     *
     */
    list_for_each(node, &graph_obj->tagged_mod_list) {
        mod = node_to_item(node, module_info_t, list);
        mod->is_configured = false;
    }

    if (dev_obj != NULL) {
        mod = NULL;
        module_info_t *add_module, *temp_mod = NULL;
        size_t module_info_size;
        struct gsl_module_id_info *module_info;
        bool mod_present = false;
        if (dev_obj->hw_ep_info.dir == AUDIO_OUTPUT)
            mod = &hw_ep_module[0];
        else
            mod = &hw_ep_module[1];

        ret = gsl_get_tagged_module_info((struct gsl_key_vector *)
                                           &meta_data_kv->gkv,
                                           mod->tag,
                                           &module_info, &module_info_size);
        if (ret != 0) {
            AGM_LOGE("cannot get tagged module info for module %x",
                          mod->tag);
            ret = -EINVAL;
            goto done;
        }
        /**
         *Check if this is the same device object as was passed for graph open
         *or a new one.We do this by comparing the module_iid of the module
         *present in the graph object with the one returned from the above api.
         *If this is a new module, we delete the older device tagged module
         *as it is not part of the graph anymore (would have been removed as a
         *part of graph_remove).
         */
        list_for_each(node, &graph_obj->tagged_mod_list) {
            temp_mod = node_to_item(node, module_info_t, list);
            if (temp_mod->miid = module_info->module_entry[0].module_iid) {
                mod_present = true;
                break;
            }
        }
        if (!mod_present) {
            /*This is a new device object, add this module to the list and
             *delete the current hw_ep(Device module) from the list.
             */
            list_for_each_safe(node, temp_node, &graph_obj->tagged_mod_list) {
                temp_mod = node_to_item(node, module_info_t, list);
                if ((temp_mod->tag = DEVICE_HW_ENDPOINT_TX) ||
                    (temp_mod->tag = DEVICE_HW_ENDPOINT_RX)) {
                    list_remove(node);
                    if (temp_mod->gkv) {
                        free(temp_mod->gkv->kv);
                        free(temp_mod->gkv);
                    }
                    free(temp_mod);
                    temp_mod = NULL;
                }
            }
            add_module = ADD_MODULE(*mod, dev_obj);
            if (!add_module) {
                AGM_LOGE("No memory to allocate for add_module\n");
                ret = -ENOMEM;
                goto done;
            }
            add_module->miid = module_info->module_entry[0].module_iid;
            add_module->mid = module_info->module_entry[0].module_id;
            /*Make a local copy of gkv and use when we query gsl
            for tagged data*/
            gkv = calloc(1, sizeof(struct agm_key_vector_gsl));
            if (!gkv) {
                AGM_LOGE("No memory to allocate for gkv\n");
                ret = -ENOMEM;
                goto done;
            }
            gkv->num_kvs = meta_data_kv->gkv.num_kvs;
            gkv->kv = calloc(gkv->num_kvs, sizeof(struct agm_key_value));
            if (!gkv->kv) {
                AGM_LOGE("No memory to allocate for kv\n");
                free(gkv);
                ret = -ENOMEM;
                goto done;
            }
            memcpy(gkv->kv, meta_data_kv->gkv.kv,
                    gkv->num_kvs * sizeof(struct agm_key_value));
            add_module->gkv = gkv;
            gkv = NULL;
        }
    }
    /*Send the new GKV for CHANGE_GRAPH*/
    change_graph.graph_key_vector.num_kvps = meta_data_kv->gkv.num_kvs;
    change_graph.graph_key_vector.kvp = (struct gsl_key_value_pair *)
                                     meta_data_kv->gkv.kv;
    change_graph.cal_key_vect.num_kvps = meta_data_kv->ckv.num_kvs;
    change_graph.cal_key_vect.kvp = (struct gsl_key_value_pair *)
                                     meta_data_kv->ckv.kv;
    ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_CHANGE_GRAPH, &change_graph,
                    sizeof(struct gsl_cmd_graph_select));
    if (ret != 0) {
        AGM_LOGE("graph add failed with error %d", ret);
        goto done;
    }
    /*configure modules again*/
    list_for_each(node, &graph_obj->tagged_mod_list) {
        mod = node_to_item(node, module_info_t, list);
        if (mod->configure) {
            ret = mod->configure(mod, graph_obj);
            if (ret != 0)
                goto done;
            mod->is_configured = true;
        }
    }
done:
    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_remove(struct graph_obj *graph_obj,
                 struct agm_meta_data_gsl *meta_data_kv)
{
    int ret = 0;
    struct gsl_cmd_remove_graph rm_graph;

    if ((graph_obj == NULL)) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }
    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %x", graph_obj->graph_handle);

    /**
     *graph_remove would only pass the graph which needs to be removed.
     *to GSL. Once graph remove is done, sesison obj will have to issue
     *graph_change/graph_add if reconfiguration of modules is needed, otherwise
     *graph_start will suffice.graph remove wont reconfigure the modules.
     */
    rm_graph.graph_key_vector.num_kvps = meta_data_kv->gkv.num_kvs;
    rm_graph.graph_key_vector.kvp = (struct gsl_key_value_pair *)
                                     meta_data_kv->gkv.kv;
    ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_REMOVE_GRAPH, &rm_graph,
                    sizeof(struct gsl_cmd_remove_graph));
    if (ret != 0) {
        AGM_LOGE("graph add failed with error %d", ret);
    }

    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_register_cb(struct graph_obj *gph_obj, event_cb cb,
                      void *client_data)
{

    if (gph_obj == NULL){
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    if (cb == NULL) {
        AGM_LOGE("No callback to register");
        return -EINVAL;
    }

    pthread_mutex_lock(&gph_obj->lock);
    gph_obj->cb = cb;
    gph_obj->client_data = client_data;
    pthread_mutex_unlock(&gph_obj->lock);

    return 0;
}

int graph_register_for_events(struct graph_obj *gph_obj,
                              struct agm_event_reg_cfg *evt_reg_cfg)
{
    int ret = 0;
    struct gsl_cmd_register_custom_event *reg_ev_payload = NULL;
    size_t payload_size = 0;

    if (gph_obj == NULL){
        AGM_LOGE("invalid graph object");
        ret = -EINVAL;
        goto done;
    }

    if (evt_reg_cfg == NULL) {
        AGM_LOGE("No event register payload passed");
        ret = -EINVAL;
        goto done;
    }
    pthread_mutex_lock(&gph_obj->lock);

    if (gph_obj->graph_handle == NULL) {
        pthread_mutex_unlock(&gph_obj->lock);
        AGM_LOGE("invalid graph handle");
        ret = -EINVAL;
        goto done;
    }
    payload_size = sizeof(struct gsl_cmd_register_custom_event) +
                                       evt_reg_cfg->event_config_payload_size;

    reg_ev_payload = calloc(1, payload_size);
    if (reg_ev_payload == NULL) {
        pthread_mutex_unlock(&gph_obj->lock);
        AGM_LOGE("calloc failed for reg_ev_payload");
        ret = -ENOMEM;
        goto done;
    }

    reg_ev_payload->event_id = evt_reg_cfg->event_id;
    reg_ev_payload->module_instance_id = evt_reg_cfg->module_instance_id;
    reg_ev_payload->event_config_payload_size =
                                 evt_reg_cfg->event_config_payload_size;
    reg_ev_payload->is_register = evt_reg_cfg->is_register;

    memcpy(reg_ev_payload + sizeof(apm_module_register_events_t),
          evt_reg_cfg->event_config_payload, evt_reg_cfg->event_config_payload_size);

    ret = gsl_ioctl(gph_obj->graph_handle, GSL_CMD_REGISTER_CUSTOM_EVENT, reg_ev_payload, payload_size);
    if (ret != 0) {
       AGM_LOGE("event registration failed with error %d", ret);
    }
    pthread_mutex_unlock(&gph_obj->lock);

done:
    return ret;
}

size_t graph_get_hw_processed_buff_cnt(struct graph_obj *graph_obj,
                                       enum direction dir)
{
    if ((graph_obj == NULL)) {
        AGM_LOGE("invalid graph object or null callback");
        return 0;
    }
    /*TODO: Uncomment that call once platform moves to latest GSL release*/
    return 2 /*gsl_get_processed_buff_cnt(graph_obj->graph_handle, dir)*/;

}

int graph_eos(struct graph_obj *graph_obj)
{
    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }
        AGM_LOGE("enter");
    return gsl_ioctl(graph_obj->graph_handle, GSL_CMD_EOS, NULL, 0);
}

int graph_get_session_time(struct graph_obj *graph_obj, uint64_t *tstamp)
{
    int ret = 0;
    uint8_t *payload = NULL;
    struct apm_module_param_data_t *header;
    struct param_id_spr_session_time_t *sess_time;
    size_t payload_size = 0;
    uint64_t timestamp;

    if (graph_obj == NULL || tstamp == NULL) {
        AGM_LOGE("Invalid Input Params");
        return -EINVAL;
    }

    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %p", graph_obj->graph_handle);
    if (!(graph_obj->state & (STARTED))) {
       AGM_LOGE("graph object is not in correct state, current state %d",
                    graph_obj->state);
       ret = -EINVAL;
       goto done;
    }
    if (graph_obj->spr_miid == 0) {
        AGM_LOGE("Invalid SPR module IID to query timestamp");
        goto done;
    }
    AGM_LOGV("SPR module IID: %x", graph_obj->spr_miid);

    payload_size = sizeof(struct apm_module_param_data_t) +
        sizeof(struct param_id_spr_session_time_t);
    /*ensure that the payloadsize is byte multiple */
    if (payload_size % 8 != 0)
        payload_size = payload_size + (8 - payload_size % 8);

    payload = calloc(1, (size_t)payload_size);
    if (!payload)
        goto done;

    header = (struct apm_module_param_data_t*)payload;
    sess_time = (struct param_id_spr_session_time_t *)
                     (payload + sizeof(struct apm_module_param_data_t));

    header->module_instance_id = graph_obj->spr_miid;
    header->param_id = PARAM_ID_SPR_SESSION_TIME;
    header->error_code = 0x0;
    header->param_size = payload_size;

    ret = gsl_get_custom_config(graph_obj->graph_handle, payload, payload_size);
    if (ret != 0) {
        AGM_LOGE("gsl_get_custom_config command failed with error %d", ret);
        goto get_fail;
    }
    AGM_LOGV("session_time: msw[%ld], lsw[%ld], at: msw[%ld], lsw[%ld] ts: msw[%ld], lsw[%ld]\n",
              sess_time->session_time.value_msw,
              sess_time->session_time.value_lsw,
              sess_time->absolute_time.value_msw,
              sess_time->absolute_time.value_lsw,
              sess_time->timestamp.value_msw,
              sess_time->timestamp.value_lsw);

    timestamp = (uint64_t)sess_time->session_time.value_msw;
    timestamp = timestamp  << 32 | sess_time->session_time.value_lsw;
    *tstamp = timestamp;

get_fail:
    free(payload);
done:
    pthread_mutex_unlock(&graph_obj->lock);
    return ret;
}
