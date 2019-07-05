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

#define LOGTAG "AGM: device"

#include <errno.h>
#include <log/log.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "device.h"
#include "metadata.h"
#include "utils.h"
#include <tinyalsa/asoundlib.h>
#include <hw_intf_cmn_api.h>

#define PCM_DEVICE_FILE "/proc/asound/pcm"

/* Global list to store supported devices */
struct device_obj **device_list;
static uint32_t num_audio_intfs;
static struct pcm_config config;

#define MAX_BUF_SIZE                 2048
#define DEFAULT_SAMPLING_RATE        48000
#define DEFAULT_PERIOD_SIZE          960
#define DEFAULT_PERIOD_COUNT         2
#define DEV_ARG_SIZE                 20
#define DEV_VALUE_SIZE               60

#define CODEC_RX0 1
#define CODEC_TX0 1
#define CODEC_RX1 2
#define CODEC_TX1 2
#define CODEC_RX2 3
#define CODEC_TX2 3
#define CODEC_RX3 4
#define CODEC_TX3 4
#define CODEC_RX4 5
#define CODEC_TX4 5
#define CODEC_RX5 6
#define CODEC_TX5 6
#define CODEC_RX6 7
#define CODEC_RX7 8

static struct pcm_config config = {
     .channels = 2,
     .rate = DEFAULT_SAMPLING_RATE,
     .period_size = DEFAULT_PERIOD_SIZE,
     .period_count = DEFAULT_PERIOD_COUNT,
     .format = PCM_FORMAT_S16_LE,
     .start_threshold = DEFAULT_PERIOD_SIZE / 4,
     .stop_threshold = INT_MAX,
};

enum pcm_format agm_to_pcm_format(enum agm_media_format format)
{
    switch (format) {
    case AGM_FORMAT_PCM_S32_LE:
        return PCM_FORMAT_S32_LE;
    case AGM_FORMAT_PCM_S8:
        return PCM_FORMAT_S8;
    case AGM_FORMAT_PCM_S24_3LE:
        return PCM_FORMAT_S24_3LE;
    case AGM_FORMAT_PCM_S24_LE:
        return PCM_FORMAT_S24_LE;
    default:
    case AGM_FORMAT_PCM_S16_LE:
        return PCM_FORMAT_S16_LE;
    };
}
int device_open(struct device_obj *dev_obj)
{
    int ret = 0;
    struct pcm *pcm = NULL;

    if (dev_obj == NULL) {
        AGM_LOGE("%s: Invalid device object\n",__func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&dev_obj->lock);
    if (dev_obj->refcnt.open) {
        AGM_LOGE("%s: PCM device %u already opened\n", __func__, dev_obj->pcm_id);
        dev_obj->refcnt.open++;
        goto done;
    }

    config.channels = dev_obj->media_config.channels;
    config.rate = dev_obj->media_config.rate;
    config.format = agm_to_pcm_format(dev_obj->media_config.format);

    pcm = pcm_open(dev_obj->card_id, dev_obj->pcm_id, dev_obj->pcm_flags,
                &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        AGM_LOGE("%s: Unable to open PCM device %u (%s)\n",
                __func__, dev_obj->pcm_id, pcm_get_error(pcm));
        ret = -EIO;
        goto done;
    }

    dev_obj->pcm = pcm;
    dev_obj->state = DEV_OPENED;
    dev_obj->refcnt.open++;
done:
    pthread_mutex_unlock(&dev_obj->lock);
    return ret;
}

static void *device_prepare_thread(void *obj)
{
    int ret = 0;
    struct device_obj *dev_obj = (struct device_obj*)obj;

    if (dev_obj == NULL) {
       AGM_LOGE("%s: Invalid device object\n",__func__);
       return NULL;
    }

    if (dev_obj->state == DEV_PREPARED) {
        AGM_LOGE("%s: device prepared already \n",__func__);
    }
      
    pthread_mutex_lock(&dev_obj->lock);
    ret = pcm_prepare(dev_obj->pcm);
    if (ret) {
        AGM_LOGE("PCM device %u prepare failed, ret = %d\n",
              __func__, dev_obj->pcm_id, ret);
        goto done;
    }

    dev_obj->state = DEV_PREPARED;
    dev_obj->refcnt.prepare++;

done:
    dev_obj->prepare_thread_created = FALSE;
    pthread_mutex_unlock(&dev_obj->lock);
    return NULL;
}

int device_prepare(struct device_obj *dev_obj)
{
    int ret = 0;
    pthread_attr_t tattr;
    struct sched_param param;

    if (dev_obj == NULL) {
        AGM_LOGE("%s: Invalid device object\n",__func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&dev_obj->lock);
    if (dev_obj->refcnt.prepare) {
        AGM_LOGD("%s: PCM device %u already in prepare state\n",
              __func__, dev_obj->pcm_id);
        dev_obj->refcnt.prepare++;
        pthread_mutex_unlock(&dev_obj->lock);
        return ret;
    }
    pthread_attr_init (&tattr);
    pthread_attr_getschedparam (&tattr, &param);
    param.sched_priority = SCHED_FIFO;
    pthread_attr_setschedparam (&tattr, &param);

    ret = pthread_create(&dev_obj->device_prepare_thread,
           (const pthread_attr_t *) &tattr, device_prepare_thread, dev_obj);
    if (ret) {
        AGM_LOGE("%s: PCM device %u prepare thread creation failed\n",
              __func__, dev_obj->pcm_id);
        dev_obj->prepare_thread_created = FALSE;
        pthread_attr_destroy(&tattr);
        pthread_mutex_unlock(&dev_obj->lock);
        return ret;
    }
    dev_obj->prepare_thread_created = TRUE;
    pthread_attr_destroy(&tattr);
    pthread_mutex_unlock(&dev_obj->lock);
    return ret;
}

int device_start(struct device_obj *dev_obj)
{
    int ret = 0;

    if (dev_obj == NULL) {
        AGM_LOGE("%s: Invalid device object\n",__func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&dev_obj->lock);
    if (dev_obj->state < DEV_PREPARED) {
        pthread_mutex_unlock(&dev_obj->lock);
        ret = pthread_join(dev_obj->device_prepare_thread, (void **) NULL);
        if (ret < 0) {
            AGM_LOGE("%s: Unable to join PCM device %u prepare thread\n",
                  __func__, dev_obj->pcm_id);
            return ret;
        }
        pthread_mutex_lock(&dev_obj->lock);
        dev_obj->prepare_thread_created = FALSE;
    }

    AGM_LOGD("%s: PCM device %u prepare thread completed\n", __func__, dev_obj->pcm_id);

    if (dev_obj->state < DEV_PREPARED) {
            AGM_LOGE("%s: PCM device %u not yet prepared, exiting\n",
                  __func__, dev_obj->pcm_id);
            ret =  -1;
            goto done;
    }

    if (dev_obj->refcnt.start) {
        AGM_LOGE("%s: PCM device %u already in start state\n",
              __func__, dev_obj->pcm_id);
        dev_obj->refcnt.start++;
        goto done;
    }

    dev_obj->state = DEV_STARTED;
    dev_obj->refcnt.start++;

done:
    pthread_mutex_unlock(&dev_obj->lock);
    return ret;
}

int device_stop(struct device_obj *dev_obj)
{
    int ret = 0;

    if(dev_obj == NULL) {
        AGM_LOGE("%s: Invalid device object\n",__func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&dev_obj->lock);
    if (!dev_obj->refcnt.start) {
        AGM_LOGE("%s: PCM device %u already stopped\n",
              __func__, dev_obj->pcm_id);
        goto done;
    }

    ret = pcm_stop(dev_obj->pcm);
    if (ret) {
        AGM_LOGE("%s: PCM device %u stop failed, ret = %d\n",
                __func__, dev_obj->pcm_id, ret);
        goto done;
    }
    dev_obj->state = DEV_STOPPED;
    dev_obj->refcnt.start--;

done:
    pthread_mutex_unlock(&dev_obj->lock);
    return ret;
}

int device_close(struct device_obj *dev_obj)
{
    int ret = 0;

    if (dev_obj == NULL) {
        AGM_LOGE("%s: Invalid device object\n",__func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&dev_obj->lock);
    if (--dev_obj->refcnt.open == 0) {
        ret = pcm_close(dev_obj->pcm);
        if (ret) {
            AGM_LOGE("%s: PCM device %u close failed, ret = %d\n",
                     __func__, dev_obj->pcm_id, ret);
            dev_obj->refcnt.open++;
            goto done;
        }
        dev_obj->state = DEV_CLOSED;
        dev_obj->refcnt.prepare = 0;
        dev_obj->refcnt.start = 0;
    }
done:
    pthread_mutex_unlock(&dev_obj->lock);
    return ret;
}

enum device_state device_current_state(struct device_obj *dev_obj)
{
    return dev_obj->state;
}

int device_get_aif_info_list(struct aif_info *aif_list, size_t *audio_intfs)
{
    struct device_obj *dev_obj;
    uint32_t copied = 0;
    uint32_t requested = *audio_intfs;

    if (*audio_intfs == 0){
        *audio_intfs = num_audio_intfs;
    } else {
        for(copied = 0; (copied < num_audio_intfs) && (copied < requested); copied++) {
            dev_obj = device_list[copied];
            strlcpy(aif_list[copied].aif_name, dev_obj->name, AIF_NAME_MAX_LEN );
            aif_list[copied].dir = dev_obj->hw_ep_info.dir;
        }
        *audio_intfs = copied;
    }
    return 0;
}

int device_get_obj(uint32_t device_idx, struct device_obj **dev_obj)
{
    if (device_idx > num_audio_intfs) {
        AGM_LOGE("%s: Invalid device_id %u, max_supported device id: %d\n",
                __func__, device_idx, num_audio_intfs);
        return -EINVAL;
    }

    *dev_obj = device_list[device_idx];
    return 0;
}

int device_get_hw_ep_info(struct device_obj *dev_obj, struct hw_ep_info *hw_ep_inf)
{
    if (dev_obj == NULL) {
        AGM_LOGE("%s: Invalid device object\n",__func__);
        return -EINVAL;
    }
    memcpy(hw_ep_inf, &(dev_obj->hw_ep_info), sizeof(struct hw_ep_info));
    return 0;
}

int populate_device_hw_ep_info(struct device_obj *dev_obj)
{
    char lpaif_type[DEV_ARG_SIZE], intf[DEV_ARG_SIZE];
    char intf_idx[DEV_ARG_SIZE], dir[DEV_ARG_SIZE];
    char arg[DEV_ARG_SIZE] = {0};
    char value[DEV_VALUE_SIZE] = {0};

    if (dev_obj == NULL) {
        AGM_LOGE("%s: Invalid device object\n",__func__);
        return -EINVAL;
    }

    if (dev_obj->name) {
        sscanf(dev_obj->name, "%20[^-]-%60s", arg, value);
        strlcpy(intf, arg, strlen(arg)+1);
        sscanf(value, "%20[^-]-%60s", arg, value);
        strlcpy(lpaif_type, arg, strlen(arg)+1);
        sscanf(value, "%20[^-]-%60s", arg, value);
        strlcpy(dir, arg, strlen(arg)+1);
        sscanf(value, "%20[^-]-%60s", arg, value);
        strlcpy(intf_idx, arg, strlen(arg)+1);
    } else {
        AGM_LOGE("%s: dev_obj_name doesn't exist\n",__func__);
        return -EINVAL;
    }

    if (strcmp(intf, "CODEC_DMA") == 0)
        dev_obj->hw_ep_info.intf = CODEC_DMA;
    else if (strcmp(intf, "MI2S") == 0)
        dev_obj->hw_ep_info.intf = MI2S;
    else if (strcmp(intf, "TDM") == 0)
        dev_obj->hw_ep_info.intf = TDM;
    else if (strcmp(intf, "AUXPCM") == 0)
        dev_obj->hw_ep_info.intf = AUXPCM;
    else if (strcmp(intf, "SLIMBUS") == 0)
        dev_obj->hw_ep_info.intf = SLIMBUS;
    else {
        AGM_LOGE("%s: No matching intf found\n",__func__);
        return -EINVAL;
    }

    if (strcmp(lpaif_type, "LPAIF") == 0)
        dev_obj->hw_ep_info.lpaif_type = LPAIF;
    else if (strcmp(lpaif_type, "LPAIF_RXTX") == 0)
        dev_obj->hw_ep_info.lpaif_type = LPAIF_RXTX;
    else if (strcmp(lpaif_type, "LPAIF_WSA") == 0)
        dev_obj->hw_ep_info.lpaif_type = LPAIF_WSA;
    else if (strcmp(lpaif_type, "LPAIF_VA") == 0)
        dev_obj->hw_ep_info.lpaif_type = LPAIF_VA;
    else if (strcmp(lpaif_type, "LPAIF_AXI") == 0)
        dev_obj->hw_ep_info.lpaif_type = LPAIF_AXI;
    else {
        AGM_LOGE("%s: No matching lpaif_type found\n",__func__);
        return -EINVAL;
    }

    if (strcmp(dir, "RX") == 0)
        dev_obj->hw_ep_info.dir = AUDIO_OUTPUT;
    else if (strcmp(dir, "TX") == 0)
        dev_obj->hw_ep_info.dir = AUDIO_INPUT;
    else {
        AGM_LOGE("%s: No matching dir found\n",__func__);
        return -EINVAL;
    }

    if (dev_obj->hw_ep_info.intf != SLIMBUS) {

        if ((strcmp(intf_idx, "PRIMARY") == 0) || (strcmp(intf_idx, "0") == 0)) {
            if (dev_obj->hw_ep_info.intf == CODEC_DMA)
                dev_obj->hw_ep_info.intf_idx = CODEC_RX0; /*RX and TX have the same value. Hence no need to check RX/TX.*/
            else
                dev_obj->hw_ep_info.intf_idx = PCM_INTF_IDX_PRIMARY;
        } else if ((strcmp(intf_idx, "SECONDARY") == 0) || (strcmp(intf_idx, "1") == 0)) {
            if (dev_obj->hw_ep_info.intf == CODEC_DMA)
                dev_obj->hw_ep_info.intf_idx = CODEC_RX1;
            else
                dev_obj->hw_ep_info.intf_idx = PCM_INTF_IDX_SECONDARY;
        } else if ((strcmp(intf_idx, "TERTIARY") == 0) || (strcmp(intf_idx, "2") == 0)) {
            if (dev_obj->hw_ep_info.intf == CODEC_DMA)
                dev_obj->hw_ep_info.intf_idx = CODEC_RX2;
            else
                dev_obj->hw_ep_info.intf_idx = PCM_INTF_IDX_TERTIARY;
        } else if ((strcmp(intf_idx, "QUATERNARY") == 0) || (strcmp(intf_idx, "3") == 0)) {
            if (dev_obj->hw_ep_info.intf == CODEC_DMA)
                dev_obj->hw_ep_info.intf_idx = CODEC_RX3;
            else
                dev_obj->hw_ep_info.intf_idx = PCM_INTF_IDX_QUATERNARY;
        } else if((strcmp(intf_idx, "QUINARY") == 0) || (strcmp(intf_idx, "4") == 0)) {
            if (dev_obj->hw_ep_info.intf == CODEC_DMA)
                dev_obj->hw_ep_info.intf_idx = CODEC_RX4;
            else
                dev_obj->hw_ep_info.intf_idx = PCM_INTF_IDX_QUINARY;
        } else {
            ALOGE("%s: No matching intf_idx found\n",__func__);
            return -EINVAL;
        }
    }
    return 0;
}

int device_set_media_config(struct device_obj *dev_obj, struct agm_media_config *device_media_config)
{
   if (dev_obj == NULL || device_media_config == NULL) {
       AGM_LOGE("%s: Invalid device object\n",__func__);
       return -EINVAL;
   }
   dev_obj->media_config.channels = device_media_config->channels;
   dev_obj->media_config.rate = device_media_config->rate;
   dev_obj->media_config.format = device_media_config->format;

   return 0;
}

int device_set_metadata(struct device_obj *dev_obj, struct agm_meta_data *metadata)
{
   return metadata_copy(&(dev_obj->metadata), metadata);
}


int device_init()
{
    char buffer[MAX_BUF_SIZE];
    unsigned int count = 0, i = 0;
    FILE *fp;
    int ret = 0;
    struct device_obj *head_obj = NULL;

    fp = fopen(PCM_DEVICE_FILE, "r");
    if (!fp) {
        ALOGE("%s: ERROR. %s file open failed", __func__, PCM_DEVICE_FILE);
        return -ENODEV;
    }

    while (fgets(buffer, MAX_BUF_SIZE - 1, fp) != NULL)
        num_audio_intfs++;

    device_list = calloc (num_audio_intfs, sizeof(struct device_obj*));
    if (!device_list) {
        ret = -ENOMEM;
        goto close_file;
    }

    rewind(fp);

    while (fgets(buffer, MAX_BUF_SIZE - 1, fp) != NULL)
    {
        struct device_obj *dev_obj = calloc(1, sizeof(struct device_obj));

        if (!dev_obj) {
            ALOGE("%s: failed to allocate device_obj mem\n", __func__);
            ret = -ENOMEM;
            goto free_device;
        }

        ALOGD("%s: buffer: %s\n", __func__, buffer);
        /* For Non-DPCM Dai-links, it is in the format of:
         * <card_num>-<pcm_device_id>: <pcm->idname> : <pcm->name> : <playback/capture> 1
         * Here, pcm->idname is in the form of "<dai_link->stream_name> <codec_name>-<num_codecs>"
         */
        sscanf(buffer, "%02u-%02u: %80s", &dev_obj->card_id, &dev_obj->pcm_id, dev_obj->name);

	      ALOGE("%d:%d:%s\n", dev_obj->card_id, dev_obj->pcm_id, dev_obj->name);
        if (strstr(buffer, "playback"))
            dev_obj->pcm_flags = PCM_OUT;
        else
            dev_obj->pcm_flags = PCM_IN;

        /* populate the hw_ep_ifo for all the available pcm-id's*/
        ret = populate_device_hw_ep_info(dev_obj);
        if (ret) {
            ALOGE("%s: ERROR. %d hw_ep_info population failed", __func__, ret);
            free(dev_obj);
            goto free_device;
        }

        pthread_mutex_init(&dev_obj->lock, (const pthread_mutexattr_t *) NULL);
        pthread_cond_init(&dev_obj->device_prepared, NULL);
        device_list[count] = dev_obj;
        count++;
        dev_obj = NULL;
    }

    num_audio_intfs = count;
    goto close_file;

free_device:
    for (i = 0; i < count; i++)
        free(device_list[i]);
    free(device_list);
close_file:
    fclose(fp);
    return ret;
}

void device_deinit()
{
    unsigned int list_count = 0;
    AGM_LOGE("%s:device deinit called\n", __func__);
    for (list_count = 0; list_count < num_audio_intfs; list_count++) {
        metadata_free(&device_list[list_count]->metadata);
        free(device_list[list_count]);
    }
    free(device_list);
    device_list = NULL;
}
