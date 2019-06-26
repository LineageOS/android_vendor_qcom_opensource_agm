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

#ifndef __AGM_DEVICE_H__
#define __AGM_DEVICE_H__

#include <pthread.h>
#include "agm_priv.h"

#define MAX_DEV_NAME_LEN     80

#define  PCM_INTF_IDX_PRIMARY       0x0
#define  PCM_INTF_IDX_SECONDARY     0x1
#define  PCM_INTF_IDX_TERTIARY      0x2
#define  PCM_INTF_IDX_QUATERNARY    0x3
#define  PCM_INTF_IDX_QUINARY       0x4

#define  CODEC_DMA                  0x0
#define  MI2S                       0x1
#define  TDM                        0x2
#define  AUXPCM                     0x3
#define  SLIMBUS                    0x4

#define  AUDIO_OUTPUT               0x1 /**< playback usecases*/
#define  AUDIO_INPUT                0x2 /**< capture/voice activation usecases*/

struct device_obj;

struct refcount {
    int open;
    int prepare;
    int start;
};

enum device_state {
    DEV_CLOSED,
    DEV_OPENED,
    DEV_PREPARED,
    DEV_STARTED,
    DEV_STOPPED,
};

typedef struct hw_ep_info
{
     /* interface e.g. CODEC_DMA, I2S, AUX_PCM, SLIMBUS…*/
     uint32_t intf;
    /* lpaif type e.g. LPAIF, LPAIF_WSA, LPAIF_RX_TX*/
     uint32_t lpaif_type;
    /* Interface ID e.g. Primary, Secondary, Tertiary ….*/
     uint32_t intf_idx;
    /* Direction of the interface RX or TX (Sink or Source)*/
     uint32_t dir;
}hw_ep_info_t;

struct device_obj {
    /* 
     * name of the device object populated from /proc/asound/pcm
     * <Interface_type>-<LPAIF_TYPE>-<DIRECTION>-<IDX>
     * <SLIM>-<DEVICEID>-<DIRECTION>-<IDX>
     * e.g.
     * TDM-LPAIF_WSA-RX-SECONDARY
     * SLIM-DEVICE_1-TX-0
     */
    char name[MAX_DEV_NAME_LEN];

    pthread_mutex_t lock;
    /* pcm device info associated with the device object */
    uint32_t card_id;
    hw_ep_info_t hw_ep_info;
    uint32_t pcm_id;
    struct pcm *pcm;
    uint32_t pcm_flags;
    struct agm_media_config media_config;
    struct agm_meta_data_gsl metadata;
    pthread_t device_prepare_thread;
    pthread_cond_t device_prepared;
    bool prepare_thread_created;

    struct refcount refcnt;
    int state;
};

/* Initializes device_obj, enumerate and fill device related information */
int device_init();
int device_deinit();
/* Returns list of supported devices */
int device_get_aif_info_list(struct aif_info *aif_list, size_t *audio_intfs);
/* returns device_obj associated with device_id */
int device_get_obj(uint32_t device_idx, struct device_obj **dev_obj);
int device_get_hw_ep_info(struct device_obj *dev_obj, struct hw_ep_info *hw_ep_info_);
int populate_device_hw_ep_info(struct device_obj *dev_obj);
int device_open(struct device_obj *dev_obj);
int device_prepare(struct device_obj *dev_obj);
int device_start(struct device_obj *dev_obj);
int device_stop(struct device_obj *dev_obj);
int device_close(struct device_obj *dev_obj);

enum device_state device_current_state(struct device_obj *obj);
/* api to set device media config */
int device_set_media_config(struct device_obj *obj, struct agm_media_config *device_media_config);
/* api to set device meta graph keys + cal keys */
int device_set_metadata(struct device_obj *obj, struct agm_meta_data *device_meta);
#endif
