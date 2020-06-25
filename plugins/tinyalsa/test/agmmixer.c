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

#include <errno.h>
#include <tinyalsa/asoundlib.h>
#include <sound/asound.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#include <agm/agm_api.h>
#include "agmmixer.h"

#define EVENT_ID_DETECTION_ENGINE_GENERIC_INFO 0x0800104F

#define PARAM_ID_DETECTION_ENGINE_GENERIC_EVENT_CFG 0x0800104E

struct apm_module_param_data_t
{
   uint32_t module_instance_id;
   uint32_t param_id;
   uint32_t param_size;
   uint32_t error_code;
};

struct detection_engine_generic_event_cfg {
    uint32_t event_mode;
};


struct gsl_module_id_info_entry {
	uint32_t module_id; /**< module id */
	uint32_t module_iid; /**< globally unique module instance id */
};

/**
 * Structure mapping the tag_id to module info (mid and miid)
 */
struct gsl_tag_module_info_entry {
	uint32_t tag_id; /**< tag id of the module */
	uint32_t num_modules; /**< number of modules matching the tag_id */
	struct gsl_module_id_info_entry module_entry[0]; /**< module list */
};

struct gsl_tag_module_info {
	uint32_t num_tags; /**< number of tags */
	struct gsl_tag_module_info_entry tag_module_entry[0];
	/**< variable payload of type struct gsl_tag_module_info_entry */
};

static unsigned int  bits_to_alsa_format(unsigned int bits)
{
    switch (bits) {
    case 32:
        return SNDRV_PCM_FORMAT_S32_LE;
    case 8:
        return SNDRV_PCM_FORMAT_S8;
    case 24:
        return SNDRV_PCM_FORMAT_S24_3LE;
    default:
    case 16:
        return SNDRV_PCM_FORMAT_S16_LE;
    };
}

int set_agm_device_media_config(struct mixer *mixer, unsigned int channels,
                                unsigned int rate, unsigned int bits, char *intf_name)
{
    char *control = "rate ch fmt";
    char *mixer_str;
    struct mixer_ctl *ctl;
    long media_config[4];
    int ctl_len = 0;
    int ret = 0;

    ctl_len = strlen(intf_name) + 1 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    snprintf(mixer_str, ctl_len, "%s %s", intf_name, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return ENOENT;
    }

    media_config[0] = rate;
    media_config[1] = channels;
    media_config[2] = bits_to_alsa_format(bits);
    media_config[3] = AGM_DATA_FORMAT_FIXED_POINT;

    printf("%s - %ld - %ld - %ld\n", __func__, media_config[0],  media_config[1], media_config[2]);
    ret = mixer_ctl_set_array(ctl, &media_config, sizeof(media_config)/sizeof(media_config[0]));
    free(mixer_str);
    return ret;
}

int connect_play_pcm_to_cap_pcm(struct mixer *mixer, unsigned int p_device, unsigned int c_device)
{
    char *pcm = "PCM";
    char *control = "loopback";
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0;
    char *val;
    int val_len = 0;
    int ret = 0;

    ctl_len = strlen(pcm) + 4 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    snprintf(mixer_str, ctl_len, "%s%d %s", pcm, c_device, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return ENOENT;
    }

    if (p_device < 0) {
        val = "ZERO";
    } else {
        val_len = strlen(pcm) + 4;
        val = calloc(1, val_len);
        snprintf(val, val_len, "%s%d", pcm, p_device);
    }

    ret = mixer_ctl_set_enum_by_string(ctl, val);
    free(mixer_str);
    if (p_device < 0)
        free(val);

    return ret;
}

int connect_agm_audio_intf_to_stream(struct mixer *mixer, unsigned int device, char *intf_name, enum stream_type stype, bool connect)
{
    char *stream = "PCM";
    char *control;
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0;
    int ret = 0;

    if (connect)
        control = "connect";
    else
        control = "disconnect";

    if (stype == STREAM_COMPRESS)
        stream = "COMPRESS";

    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    snprintf(mixer_str, ctl_len, "%s%d %s", stream, device, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return ENOENT;
    }

    ret = mixer_ctl_set_enum_by_string(ctl, intf_name);
    free(mixer_str);
    return ret;
}

int agm_mixer_set_ecref_path(struct mixer *mixer, unsigned int device, enum stream_type stype, char *intf_name)
{
    char *stream = "PCM";
    char *control;
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0;
    int ret = 0;

    control = "echoReference";

    if (stype == STREAM_COMPRESS)
        stream = "COMPRESS";

    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    snprintf(mixer_str, ctl_len, "%s%d %s", stream, device, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return ENOENT;
    }

    ret = mixer_ctl_set_enum_by_string(ctl, intf_name);
    free(mixer_str);
    return ret;
}

int set_agm_audio_intf_metadata(struct mixer *mixer, char *intf_name, enum dir d, int rate, int bitwidth)
{
    char *control = "metadata";
    struct mixer_ctl *ctl;
    char *mixer_str;
    struct agm_key_value *gkv = NULL, *ckv = NULL;
    struct prop_data *prop = NULL;
    uint8_t *metadata = NULL;
    uint32_t num_gkv = 1, num_ckv = 2, num_props = 0;
    uint32_t gkv_size, ckv_size, prop_size, ckv_index = 0;
    int ctl_len = 0, offset = 0;
    int ret = 0;

    gkv_size = num_gkv * sizeof(struct agm_key_value);
    ckv_size = num_ckv * sizeof(struct agm_key_value);
    prop_size = sizeof(struct prop_data) + (num_props * sizeof(uint32_t));

    metadata = calloc(1, sizeof(num_gkv) + sizeof(num_ckv) + gkv_size + ckv_size + prop_size);
    if (!metadata)
        return -ENOMEM;

    gkv = calloc(num_gkv, sizeof(struct agm_key_value));
    ckv = calloc(num_ckv, sizeof(struct agm_key_value));
    prop = calloc(1, prop_size);
    if (!gkv || !ckv || !prop) {
        if (ckv)
            free(ckv);
        if (gkv)
            free(gkv);
        free(metadata);
        return -ENOMEM;
    }

    if (d == PLAYBACK) {
        gkv[0].key = DEVICERX;
        gkv[0].value = SPEAKER;
    } else {
        gkv[0].key = DEVICETX;
        gkv[0].value = HANDSETMIC;
    }
    ckv[ckv_index].key = SAMPLINGRATE;
    ckv[ckv_index].value = rate;

    ckv_index++;
    ckv[ckv_index].key = BITWIDTH;
    ckv[ckv_index].value = bitwidth;

    prop->prop_id = 0;  //Update prop_id here
    prop->num_values = num_props;

    memcpy(metadata, &num_gkv, sizeof(num_gkv));
    offset += sizeof(num_gkv);
    memcpy(metadata + offset, gkv, gkv_size);
    offset += gkv_size;
    memcpy(metadata + offset, &num_ckv, sizeof(num_ckv));
    offset += sizeof(num_ckv);
    memcpy(metadata + offset, ckv, ckv_size);
    offset += ckv_size;
    memcpy(metadata + offset, prop, prop_size);

    ctl_len = strlen(intf_name) + 1 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    if (!mixer_str) {
        free(metadata);
        return -ENOMEM;
    }
    snprintf(mixer_str, ctl_len, "%s %s", intf_name, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(gkv);
        free(ckv);
        free(prop);
        free(metadata);
        free(mixer_str);
        return ENOENT;
    }

    ret = mixer_ctl_set_array(ctl, metadata, sizeof(num_gkv) + sizeof(num_ckv) + gkv_size + ckv_size + prop_size);

    free(gkv);
    free(ckv);
    free(prop);
    free(metadata);
    free(mixer_str);
    return ret;
}

int set_agm_stream_metadata_type(struct mixer *mixer, int device, char *val, enum stream_type stype)
{
    char *stream = "PCM";
    char *control = "control";
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0,ret = 0;

    if (stype == STREAM_COMPRESS)
        stream = "COMPRESS";

    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    snprintf(mixer_str, ctl_len, "%s%d %s", stream, device, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return ENOENT;
    }

    ret = mixer_ctl_set_enum_by_string(ctl, val);
    free(mixer_str);
    return ret;
}

int set_agm_stream_metadata(struct mixer *mixer, int device, uint32_t val, enum stream_type stype, char *intf_name)
{
    char *stream = "PCM";
    char *control = "metadata";
    char *mixer_str;
    struct mixer_ctl *ctl;
    uint8_t *metadata = NULL;
    struct agm_key_value *gkv = NULL, *ckv = NULL;
    struct prop_data *prop = NULL;
    uint32_t num_gkv = 1, num_ckv = 1, num_props = 0;
    uint32_t gkv_size, ckv_size, prop_size, index = 0;
    int ctl_len = 0, ret = 0, offset = 0;
    char *type = "ZERO";

    if (intf_name)
        type = intf_name;

    printf("%s type  = %s\n", __func__, type);

    ret = set_agm_stream_metadata_type(mixer, device, type, stype);
    if (ret)
        return ret;

    if (stype == STREAM_COMPRESS){
        stream = "COMPRESS";
        gkv[index].key = STREAMRX;
    }

    if (val == PCM_LL_PLAYBACK || val == COMPRESSED_OFFLOAD_PLAYBACK){
        num_gkv = 2;
        gkv[index].key = STREAMRX;
    }

    if (val == VOICE_UI && intf_name){
        num_gkv = 2;
        gkv[index].key = STREAMTX;
    }

    gkv_size = num_gkv * sizeof(struct agm_key_value);
    ckv_size = num_ckv * sizeof(struct agm_key_value);
    prop_size = sizeof(struct prop_data) + (num_props * sizeof(uint32_t));

    metadata = calloc(1, sizeof(num_gkv) + sizeof(num_ckv) + gkv_size + ckv_size + prop_size);
    if (!metadata)
        return -ENOMEM;

    gkv = calloc(num_gkv, sizeof(struct agm_key_value));
    ckv = calloc(num_ckv, sizeof(struct agm_key_value));
    prop = calloc(1, prop_size);
    if (!gkv || !ckv || !prop) {
        if (ckv)
            free(ckv);
        if (gkv)
            free(gkv);
        free(metadata);
        return -ENOMEM;
    }

    gkv[index].value = val;

    index++;
    if (val == PCM_LL_PLAYBACK || val == COMPRESSED_OFFLOAD_PLAYBACK) {
        gkv[index].key = INSTANCE;
        gkv[index].value = INSTANCE_1;
    }

    if (val == VOICE_UI && intf_name) {
        gkv[index].key = DEVICEPP_TX;
        gkv[index].value = DEVICEPP_TX_VOICE_UI_FLUENCE_FFECNS;
    }

    index = 0;
    ckv[index].key = STREAMTX;
    ckv[index].value = val;

    prop->prop_id = 0;  //Update prop_id here
    prop->num_values = num_props;

    memcpy(metadata, &num_gkv, sizeof(num_gkv));
    offset += sizeof(num_gkv);
    memcpy(metadata + offset, gkv, gkv_size);
    offset += gkv_size;
    memcpy(metadata + offset, &num_ckv, sizeof(num_ckv));

    offset += sizeof(num_ckv);
    memcpy(metadata + offset, ckv, ckv_size);
    offset += ckv_size;
    memcpy(metadata + offset, prop, prop_size);

    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    if (!mixer_str) {
        free(metadata);
        return -ENOMEM;
    }
    snprintf(mixer_str, ctl_len, "%s%d %s", stream, device, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(gkv);
        free(ckv);
        free(prop);
        free(metadata);
        free(mixer_str);
        return ENOENT;
    }

    ret = mixer_ctl_set_array(ctl, metadata, sizeof(num_gkv) + sizeof(num_ckv) + gkv_size + ckv_size + prop_size);

    free(gkv);
    free(ckv);
    free(prop);
    free(metadata);
    free(mixer_str);
    return ret;
}

int agm_mixer_register_event(struct mixer *mixer, int device, enum stream_type stype, uint32_t miid, uint8_t is_register)
{
    char *stream = "PCM";
    char *control = "event";
    char *mixer_str;
    struct mixer_ctl *ctl;
    struct agm_event_reg_cfg *event_cfg;
    int payload_size = 0;
    int ctl_len = 0,ret = 0;

    if (stype == STREAM_COMPRESS)
        stream = "COMPRESS";

    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    if (!mixer_str)
        return -ENOMEM;

    snprintf(mixer_str, ctl_len, "%s%d %s", stream, device, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return ENOENT;
    }

    ctl_len = sizeof(struct agm_event_reg_cfg) + payload_size;
    event_cfg = calloc(1, ctl_len);
    if (!event_cfg) {
        free(mixer_str);
        return -ENOMEM;
    }

    event_cfg->module_instance_id = miid;
    event_cfg->event_id = EVENT_ID_DETECTION_ENGINE_GENERIC_INFO;
    event_cfg->event_config_payload_size = payload_size;
    event_cfg->is_register = !!is_register;

    ret = mixer_ctl_set_array(ctl, event_cfg, ctl_len);
    free(event_cfg);
    free(mixer_str);
    return ret;
}

int agm_mixer_get_miid(struct mixer *mixer, int device, char *intf_name,
                       enum stream_type stype, int tag_id, uint32_t *miid)
{
    char *stream = "PCM";
    char *control = "getTaggedInfo";
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0,ret = 0, i;
    void *payload;
    struct gsl_tag_module_info *tag_info;
    struct gsl_tag_module_info_entry *tag_entry;
    int offset = 0;

    ret = set_agm_stream_metadata_type(mixer, device, intf_name, stype);
    if (ret)
        return ret;

    if (stype == STREAM_COMPRESS)
        stream = "COMPRESS";

    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    if (!mixer_str)
        return -ENOMEM;

    snprintf(mixer_str, ctl_len, "%s%d %s", stream, device, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return ENOENT;
    }

    payload = calloc(1024, sizeof(char));
    if (!payload) {
        free(mixer_str);
        return -ENOMEM;
    }

    ret = mixer_ctl_get_array(ctl, payload, 1024);
    if (ret < 0) {
        printf("Failed to mixer_ctl_get_array\n");
        free(payload);
        free(mixer_str);
        return ret;
    }
    tag_info = payload;
    printf("%s num of tags associated with stream %d is %d\n", __func__, device, tag_info->num_tags);
    ret = -1;
    tag_entry = &tag_info->tag_module_entry[0];
    offset = 0;
    for (i = 0; i < tag_info->num_tags; i++) {
        tag_entry += offset/sizeof(struct gsl_tag_module_info_entry);

        printf("%s tag id[%d] = %lx, num_modules = %lx\n", __func__, i, (unsigned long) tag_entry->tag_id, (unsigned long) tag_entry->num_modules);
        offset = sizeof(struct gsl_tag_module_info_entry) + (tag_entry->num_modules * sizeof(struct gsl_module_id_info_entry));
        if (tag_entry->tag_id == tag_id) {
            struct gsl_module_id_info_entry *mod_info_entry;

            if (tag_entry->num_modules) {
                 mod_info_entry = &tag_entry->module_entry[0];
                 *miid = mod_info_entry->module_iid;
                 printf("MIID is %x\n", *miid);
                 ret = 0;
                 break;
            }
        }
    }

    free(payload);
    free(mixer_str);
    return ret;
}

int agm_mixer_set_param(struct mixer *mixer, int device,
                        enum stream_type stype, void *payload, int size)
{
    char *stream = "PCM";
    char *control = "setParam";
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0,ret = 0;

    if (stype == STREAM_COMPRESS)
        stream = "COMPRESS";


    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    if (!mixer_str) {
        free(payload);
        return -ENOMEM;
    }
    snprintf(mixer_str, ctl_len, "%s%d %s", stream, device, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return ENOENT;
    }


    ret = mixer_ctl_set_array(ctl, payload, size);

    printf("%s %d, cnt %d\n", __func__, ret, size);
    free(mixer_str);
    return ret;
}

int agm_mixer_set_param_with_file(struct mixer *mixer, int device,
                                  enum stream_type stype, char *path)
{
    FILE *fp;
    int size, bytes_read;
    void *payload;
    char *stream = "PCM";
    char *control = "setParam";
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0,ret = 0;

    if (stype == STREAM_COMPRESS)
        stream = "COMPRESS";

    fp = fopen(path, "rb");
    if (!fp) {
        printf("%s: Unable to open file '%s'\n", __func__, path);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    printf("%s: size of %s file is %d bytes\n", __func__, path, size);

    payload = calloc(1, size);
    if (!payload)
        return -ENOMEM;

    fseek(fp, 0, SEEK_SET);
    bytes_read = fread((char*)payload, 1, size , fp);
    if (bytes_read != size) {
        printf("%s failed to read data from file %s, bytes read = %d\n", __func__, path, bytes_read);
        free(payload);
        return -1;
    }

    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    if (!mixer_str) {
        free(payload);
        fclose(fp);
        return -ENOMEM;
    }
    snprintf(mixer_str, ctl_len, "%s%d %s", stream, device, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        free(payload);
        fclose(fp);
        return ENOENT;
    }


    ret = mixer_ctl_set_array(ctl, payload, size);

    printf("%s %d, cnt %d\n", __func__, ret, size);
    free(mixer_str);
    free(payload);
    fclose(fp);
    return ret;
}

int agm_mixer_get_event_param(struct mixer *mixer, int device, enum stream_type stype, uint32_t miid)
{
    char *stream = "PCM";
    char *control = "getParam";
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0,ret = 0;
    struct apm_module_param_data_t* header;
    struct detection_engine_generic_event_cfg *pEventCfg;
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    int i;

    if (stype == STREAM_COMPRESS)
        stream = "COMPRESS";

    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    if (!mixer_str)
        return -ENOMEM;

    snprintf(mixer_str, ctl_len, "%s%d %s", stream, device, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return ENOENT;
    }


    payloadSize = sizeof(struct apm_module_param_data_t) +
                  sizeof(struct detection_engine_generic_event_cfg);

    if (payloadSize % 8 != 0)
        payloadSize = payloadSize + (8 - payloadSize % 8);

    payload = (uint8_t*)malloc((size_t)payloadSize);
    header = (struct apm_module_param_data_t*)payload;
    header->module_instance_id = miid;
    header->param_id = PARAM_ID_DETECTION_ENGINE_GENERIC_EVENT_CFG;
    header->error_code = 0x0;
    header->param_size = payloadSize -  sizeof(struct apm_module_param_data_t);

    pEventCfg = (struct detection_engine_generic_event_cfg *)
                (payload + sizeof(struct apm_module_param_data_t));

    ret = mixer_ctl_set_array(ctl, payload, payloadSize);
    if (ret) {
         printf("%s set failed\n", __func__);
         goto exit;
    }

    for (i = 0; i< payloadSize; i++)
        printf("0x%x ",payload[i]);

    printf("\n");
    memset(payload, 0, payloadSize);
    ret = mixer_ctl_get_array(ctl, payload, payloadSize);
    if (ret) {
         printf("%s set failed\n", __func__);
         goto exit;
    }

    printf("received payload data is 0x%x\n", pEventCfg->event_mode);
    for (i = 0; i< payloadSize; i++)
        printf("0x%x ",payload[i]);

    printf("\n");
exit:
    free(mixer_str);
    free(payload);
    return ret;
}

int agm_mixer_get_buf_tstamp(struct mixer *mixer, int device, enum stream_type stype, uint64_t *tstamp)
{
    char *stream = "PCM";
    char *control = "bufTimestamp";
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0,ret = 0;
    uint64_t ts = 0;

    if (stype == STREAM_COMPRESS)
        stream = "COMPRESS";

    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    if (!mixer_str)
        return -ENOMEM;

    snprintf(mixer_str, ctl_len, "%s%d %s", stream, device, control);

    printf("%s - mixer -%s-\n", __func__, mixer_str);
    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return ENOENT;
    }

    ret = mixer_ctl_get_array(ctl, &ts, sizeof(uint64_t));
    if (ret) {
         printf("%s get failed\n", __func__);
         goto exit;
    }

    printf("received timestamp is 0x%llx\n", (unsigned long long) ts);
    *tstamp = ts;
exit:
    free(mixer_str);
    return ret;
}
