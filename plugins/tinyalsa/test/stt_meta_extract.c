/*
* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
* Copyright (c) 2021,2023,2025 Qualcomm Innovation Center, Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above
*      copyright notice, this list of conditions and the following
*      disclaimer in the documentation and/or other materials provided
*      with the distribution.
*    * Neither the name of The Linux Foundation nor the names of its
*      contributors may be used to endorse or promote products derived
*      from this software without specific prior written permission.
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

/* Test app to capture STT data using ctls from kernel */

#include <tinyalsa/asoundlib.h>
#include <errno.h>
#include <math.h>
#include "stt_meta_extract.h"
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include "agmmixer.h"

#define nullptr NULL
#define PARAM_ID_FLUENCE_NN_SOURCE_TRACKING_MONITOR 0x08001516

struct apm_module_param_data_t
{
   uint32_t module_instance_id;
   uint32_t param_id;
   uint32_t param_size;
   uint32_t error_code;
};

static struct stt_meta_info g_stt_meta;

static void stt_meta_init(void)
{
    memset(&g_stt_meta, 0x0, sizeof(struct stt_meta_info));

    g_stt_meta.fv = FV_13;
    g_stt_meta.card = 100;
    g_stt_meta.device = 101;

    memset(&g_stt_meta.be_intf, 0x0, BE_INTF_MAX);
    memcpy(g_stt_meta.be_intf, "CODEC_DMA-LPAIF_RXTX-TX-3", strlen("CODEC_DMA-LPAIF_RXTX-TX-3"));

    g_stt_meta.get_data_iter = 1, 
    g_stt_meta.get_data_time_gap = RETRY_US;

    g_stt_meta.log_file = stdout;

    g_stt_meta.is_audio_reach = false;
    g_stt_meta.is_source_track_get = true;
    g_stt_meta.is_sound_focus_get = false;
    g_stt_meta.is_sound_focus_set = false;
}

static int elite_get_sourcetrack_metadata(void *payload, unsigned int meta_size,
                                                  struct mixer_ctl *ctl) {

    int ret = 0;
    unsigned int count;

    if (!ctl || !payload) {
        printf("%s: Invalid params, ctl / source track payload is NULL", __func__);
        return -EINVAL;
    }

    printf("%s from mixer", __func__);
    mixer_ctl_update(ctl);

    count = mixer_ctl_get_num_values(ctl);

    if (count != meta_size) {
        printf("%s: mixer_ctl_get_num_values() invalid source tracking data size %d",
                                                                    __func__, count);
        ret = -EINVAL;
        goto done;
    }

    ret = mixer_ctl_get_array(ctl, payload, count);

    if (ret != 0) {
        printf("%s: mixer_ctl_get_array() failed to get Source Tracking Params", __func__);
        ret = -EINVAL;
        goto done;
    }

done:
    printf("%s exit with %d", __func__, ret);
    return ret;
}

static int elite_get_soundfocus_metadata(struct sound_focus_meta *sound_focus_meta,
                                                     struct mixer_ctl *ctl) {
    int ret = 0, count;
    struct timespec ts;

    if (!ctl) {
        printf("%s: not a valid ctrl", __func__);
        return -EINVAL;
    } else {
        printf("%s: from mixer", __func__);
        mixer_ctl_update(ctl);
        count = mixer_ctl_get_num_values(ctl);

        if (count != (sizeof(struct sound_focus_meta) - sizeof(ts))) {
            printf("%s: mixer_ctl_get_num_values() invalid sound focus data size %d",
                                                                           __func__, count);
            ret = -EINVAL;
            goto done;
        }

        ret = mixer_ctl_get_array(ctl, (void *)sound_focus_meta, count);
        if (ret != 0) {
            printf("%s: mixer_ctl_get_array() failed to get Sound Focus Params", __func__);
            ret = -EINVAL;
            goto done;
        }

        clock_gettime(CLOCK_MONOTONIC, &ts);
        sound_focus_meta->ts = ts;
    }

done:
    printf("%s exit with %d", __func__, ret);
    return ret;
}

static int elite_set_soundfocus_metadata(struct sound_focus_meta *sound_focus_meta,
                                                     struct mixer_ctl *ctl) {
    int ret = 0, count;
    struct timespec ts;

    if (!ctl) {
        printf("%s: not a valid ctrl", __func__);
        return -EINVAL;
    } else {
        printf("%s: Setting Sound Focus Params func", __func__);
        mixer_ctl_update(ctl);
        count = mixer_ctl_get_num_values(ctl);

        if (count != (sizeof(struct sound_focus_meta) - sizeof(ts))) {
            printf("%s: mixer_ctl_get_num_values() invalid sound focus data size %d",
                                                                           __func__, count);
            ret = -EINVAL;
            goto done;
        }

        ret = mixer_ctl_set_array(ctl, (void *)sound_focus_meta, count);
        if (ret != 0) {
            printf("%s: mixer_ctl_set_array() failed to set Sound Focus Params", __func__);
            ret = -EINVAL;
            goto done;
        }
    }

done:
    printf("%s exit with %d", __func__, ret);
    return ret;
}

static void elite_updatestt_data(char* str, struct sound_focus_meta *sound_focus_meta) {
    int str_len = strlen(str);
    int j = 0, i;
    int *arr=(int*)malloc(str_len*sizeof(int));

    for (i = 0; str[i] != '\0'; i++) {
        if (str[i] == ',')
            j++;
        else
            arr[j] = arr[j] * DECI + (str[i] - ASCI_NUM);
    }
    j=0;
    for (i = 0; i < NUM_SECTORS; i++) {
        sound_focus_meta->start_angle[i] = arr[j++];
    }
    for (i = 0; i < NUM_SECTORS; i++) {
        sound_focus_meta->enable[i] = arr[j++];
    }
    sound_focus_meta->gain_step = arr[j];
}

static void usage() {
    printf(" \n Command \n");
    printf(" \n stt_meta_extract <options>\n");
    printf(" \n Options\n");
    printf(" -t  --source_track_data         - get source tracking params data with recording\n");
    printf(" -f  --sound_focus_data          - get sound focus params data with recording\n");
    printf(" -s  --sound focus set           - set sound focus params data with recording\n\n");
    printf(" -g  --meta-time-gap             - time between successive get data in msec\n\n");
    printf(" -n  --get-data-iterations       - get iterations cnt, (-ve) for cont get data\n\n");
    printf(" -b  --audio be interface        - update audio back end interface\n\n");
    printf(" -p  --audio virtual pcm index   - update virtual pcm index\n\n");
    printf(" -d  --sound card index          - update sound card index\n\n");
    printf(" -l  --log file_name             - output log file\n\n");
    printf(" -h  --help                      - Show this help\n\n");
    printf(" \n Examples \n");
    printf(" stt_meta_extract      -> Get source track meta data while record in progress\n\n");
    printf(" stt_meta_extract -t 1 -> Get source track meta data while record in progress\n\n");
    printf(" stt_meta_extract -f 1 -> Get sound focus meta data while record in progress\n\n");
    printf(" stt_meta_extract -s 45,110,235,310,1,0,0,1,50 -> Set sound focus param data\n");
    printf("                                    secort_startangles[4],secotr_enable[4],gain \n\n");
    printf(" stt_meta_extract -g 200 -n 5 ->    Get stt meta data 5 times for every 200msec\n\n");
    printf(" stt_meta_extract -b TX_CDC_DMA_TX_3 -> Get stt meta with be TX_CDC_DMA_TX_3 \n\n");
}

int agm_mixer_get_sound_track_info(struct mixer *mixer, int device, enum stream_type stype, uint32_t miid)
{
    char *stream = "PCM";
    char *control = "getParam";
    char *mixer_str;
    struct mixer_ctl *ctl;
    int ctl_len = 0,ret = 0;
    struct apm_module_param_data_t* header;
    uint8_t* payload = NULL;
    int idx;
    struct source_track_meta_fnn *config = NULL;
    uint8_t *ptr = NULL;
    size_t payloadSize = 0;
    size_t configSize = 0;
    int i = 0;
    FILE * log_file = g_stt_meta.log_file;

    if (stype == STREAM_COMPRESS)
        stream = "COMPRESS";

    ctl_len = strlen(stream) + 4 + strlen(control) + 1;
    mixer_str = calloc(1, ctl_len);
    if (!mixer_str)
        return -ENOMEM;

    snprintf(mixer_str, ctl_len, "%s%d %s", stream, device, control);

    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if (!ctl) {
        printf("Invalid mixer control: %s\n", mixer_str);
        free(mixer_str);
        return ENOENT;
    }

    printf("%s mixer_str: %s\n", __func__, mixer_str);

    // search in TX GKV
    payloadSize = sizeof(struct apm_module_param_data_t) +
                  sizeof(struct source_track_meta_fnn);

    if (payloadSize % 8 != 0)
        payloadSize = payloadSize + (8 - payloadSize % 8);

    payload = (uint8_t*)malloc((size_t)payloadSize);
    if (!payload) {
        printf("malloc failed\n");
        free(mixer_str);
        return -ENOMEM;
    }
    header = (struct apm_module_param_data_t*)payload;
    header->module_instance_id = miid;
    header->param_id = PARAM_ID_FLUENCE_NN_SOURCE_TRACKING_MONITOR;
    header->error_code = 0x0;
    header->param_size = payloadSize -  sizeof(struct apm_module_param_data_t);

    printf("header params IID:%#x param_id:%#x error_code:%d param_size:%d\n",
                  header->module_instance_id, header->param_id,
                  header->error_code, header->param_size);

    ret = mixer_ctl_set_array(ctl, payload, payloadSize);
    if (ret) {
         printf("%s set custom config failed, ret = %d\n", __func__, ret);
         goto exit;
    }

    ret = mixer_ctl_get_array(ctl, payload, payloadSize);
    if (ret) {
         printf("%s get custom config failed, ret = %d", __func__, ret);
         goto exit;
    }

    configSize = sizeof(struct source_track_meta_fnn);
    ptr = (uint8_t *)payload + sizeof(struct apm_module_param_data_t);
    config = (uint8_t *)calloc(1, configSize);
    if (!config) {
        printf("%s Failed to allocate memory for config\n", __func__);
        ret = -ENOMEM;
        goto exit;
    }
    memcpy(config, ptr, configSize);

    /* Print meta data */
    fprintf(log_file, "time stamp session_time_lsw %u session_time_msw %u \n",
    config->session_time_lsw, config->session_time_msw);

    for (idx = 0; idx < TOTAL_SPEAKERS; idx++)
        fprintf(log_file, "speakers[%d]=%d ",idx, config->speakers[idx]);

    fprintf(log_file, "\nreserved=%d\n",config->reserved);

    for (idx = 0; idx < TOTAL_DEGREES; idx++)
        fprintf(log_file, "polarActivity[%d]=%d ",idx, config->polarActivity[idx]);

    printf("\nreceived payload data, successful\n");
exit:
    if (mixer_str)
        free(mixer_str);
    if (payload)
        free(payload);
    if (config)
        free(config);
    return ret;
}

static unsigned int audioreach_get_soundtrack_metadata(unsigned int card, unsigned int device, char *intf_name)
{
    struct mixer *mixer;
    uint32_t miid = 0;
    int ret = 0;

    mixer = mixer_open(card);
    if (!mixer) {
        printf("Failed to open mixer\n");
        return 0;
    }

    // get fluenceNN miid
    ret = agm_mixer_get_miid (mixer, device, intf_name, STREAM_PCM, TAG_ECNS, &miid);
    if (ret) {
        printf("ECNS not present for this graph: agm_mixer_get_miid() failed, tagId: %#x, ret; %d\n", TAG_ECNS, ret);
        goto exit;
    } else {
        printf("ECNS miid:%#x\n", miid);
    }

    // get sound tracking meta info/data
    agm_mixer_get_sound_track_info(mixer, device, STREAM_PCM, miid);
    if (ret) {
        printf("get sound track info failed: %d\n", ret);
    }

exit:
    mixer_close(mixer);
    return ret;
}

static int elite_derive_mixer_ctl_stt(struct mixer **stt_mixer, struct mixer_ctl **ctl_st, struct mixer_ctl **ctl_sf,
                                 struct mixer_ctl **ctl_st_fnn, char* be_intf, enum fluence_version fversion) {

    char sound_focus_mixer_ctl_name[MIXER_PATH_MAX_LENGTH] = "Sound Focus Audio Tx ";
    char source_tracking_mixer_ctl_name[MIXER_PATH_MAX_LENGTH] = "Source Tracking Audio Tx ";
    char st_fnn_mixer_ctl_name[MIXER_PATH_MAX_LENGTH] = "FNN STM Audio Tx ";
    char fluence_property[PROPERTY_VALUE_MAX];
    struct mixer *mixer = NULL;
    int ret = 0, retry_num = 0;
    struct mixer_ctl *ctl = NULL;

    if (!stt_mixer) {
        printf("%s: invalid mixer", __func__);
        return -EINVAL;
    }

    if (!ctl_st) {
        printf("%s: invalid Source tracking mixer ctl", __func__);
        return -EINVAL;
    }

    if (!ctl_sf) {
        printf("%s: invalid Sound focus mixer ctl", __func__);
        return -EINVAL;
    }

    if (!ctl_st_fnn) {
        printf("%s: invalid Sound tracking mixer ctl for fnn", __func__);
        return -EINVAL;
    }

    strlcat(sound_focus_mixer_ctl_name, be_intf, MIXER_PATH_MAX_LENGTH);
    strlcat(source_tracking_mixer_ctl_name, be_intf, MIXER_PATH_MAX_LENGTH);
    strlcat(st_fnn_mixer_ctl_name, be_intf, MIXER_PATH_MAX_LENGTH);

    mixer = mixer_open(SOUND_CARD);

    while (!mixer && retry_num < MIXER_OPEN_MAX_NUM_RETRY) {
        usleep(RETRY_US);
        mixer = mixer_open(SOUND_CARD);
        retry_num++;
    }

    if (!mixer) {
        printf("%s: ERROR. Unable to open the mixer, aborting", __func__);
        ret = -EINVAL;
        goto clean;
    } else {
        *stt_mixer = mixer;
    }

    switch (fversion) {
        case FV_11:
        {
            ctl = mixer_get_ctl_by_name(mixer, source_tracking_mixer_ctl_name);

            if (!ctl) {
                printf("%s: Could not get ctl for mixer cmd - %s",
                        __func__, source_tracking_mixer_ctl_name);
                ret = -EINVAL;
                goto clean;
            } else {
                *ctl_st = ctl;
            }

            ctl = mixer_get_ctl_by_name(mixer, sound_focus_mixer_ctl_name);
            if (!ctl) {
                printf("%s: Could not get ctl for mixer cmd - %s",
                        __func__, source_tracking_mixer_ctl_name);
                ret = -EINVAL;
                goto clean;
            } else {
                *ctl_sf = ctl;
            }
            break;
        }
        case FV_13:
        {
            ctl = mixer_get_ctl_by_name(mixer, st_fnn_mixer_ctl_name);
            if (!ctl) {
                printf("%s: Could not get ctl for mixer cmd - %s",
                        __func__, st_fnn_mixer_ctl_name);
                ret = -EINVAL;
                goto clean;
            } else {
                *ctl_st_fnn = ctl;
            }
            break;
        }
        default:
            printf("%s: invalid fluence type", __func__);
            break;
    }

    return ret;

clean:
    if (mixer)
        mixer_close(mixer);

    return ret;
}


static int elite_stt_meta_extract(void)
{
    int idx, count, sect, ret = 0;
    struct timespec ts;

    struct sound_focus_meta sound_focus_metadataset;
    struct source_track_meta source_track_metadata;
    struct sound_focus_meta sound_focus_metadata;
    struct source_track_meta_fnn source_track_metadata_fnn;

    /* Open mixer for snd card 0 */
    struct mixer *stt_mixer = NULL;
    struct mixer_ctl *ctl_st = NULL, *ctl_sf = NULL, *ctl_st_fnn = NULL;

    bool is_source_track_get = g_stt_meta.is_source_track_get;
    bool is_sound_focus_get = g_stt_meta.is_sound_focus_get;
    bool is_sound_focus_set = g_stt_meta.is_sound_focus_set;

    unsigned int card = g_stt_meta.card;
    unsigned int device = g_stt_meta.device;
    enum fluence_version fv = g_stt_meta.fv;

    int get_data_iter = g_stt_meta.get_data_iter;
    int get_data_time_gap = g_stt_meta.get_data_time_gap;

    FILE * log_file = g_stt_meta.log_file;

    memset(&sound_focus_metadataset, 0x0, sizeof(struct sound_focus_meta));
    memset(&source_track_metadata_fnn, 0xFF, sizeof(struct source_track_meta_fnn));
    memset(&source_track_metadata, 0xFF, sizeof(struct source_track_meta));
    memset(&sound_focus_metadata, 0x0, sizeof(struct sound_focus_meta));

    ret = elite_derive_mixer_ctl_stt(&stt_mixer, &ctl_st, &ctl_sf, &ctl_st_fnn, g_stt_meta.be_intf, fv);
    if (ret != 0) {
        printf("failed to derive mixer controls %d", ret);
        goto done;
    }

    fprintf(log_file, "get_source_track meta data with gap of %d us \n", get_data_time_gap);

    if (is_sound_focus_set) {
        if (fv != FV_11) {
            printf("sound_focus set is not supported for fluence version %d ", fv);
            ret = -1;
            goto done;
        }

        memcpy(&sound_focus_metadataset, &g_stt_meta.sound_focus_metadataset, sizeof(struct sound_focus_meta));

        ret = elite_set_soundfocus_metadata(&sound_focus_metadataset, ctl_sf);

        if (ret != 0) {
            printf("failed to set soundfocus metadata %d", ret);
            goto done;
        }

        ret = elite_get_soundfocus_metadata(&sound_focus_metadata, ctl_sf);

        if (ret != 0) {
            printf("failed to get soundfocus metadata %d", ret);
            goto done;
        }

    }

    while (get_data_iter != 0) {
        switch (fv) {
            case FV_13:
            {
                if (is_source_track_get) {
                    ret = elite_get_sourcetrack_metadata((void *)&source_track_metadata_fnn,
                          sizeof(struct source_track_meta_fnn), ctl_st_fnn);
                    if (ret != 0) {
                        printf("failed to get source track meta data %d", ret);
                        goto done;
                    }

                }

                /* Print meta data */
                fprintf(log_file, "time stamp session_time_lsw %u session_time_msw %u \n",
                source_track_metadata_fnn.session_time_lsw,source_track_metadata_fnn.session_time_msw);

                for (idx = 0; idx < TOTAL_SPEAKERS; idx++)
                    printf("speakers[%d]=%d ",idx, source_track_metadata_fnn.speakers[idx]);

                fprintf(log_file, "\nreserved=%d\n",source_track_metadata_fnn.reserved);

                for (idx = 0; idx < TOTAL_DEGREES; idx++)
                    printf("polarActivity[%d]=%d ",idx, source_track_metadata_fnn.polarActivity[idx]);

                break;
            }
            case FV_11:
            {
                if (is_sound_focus_get) {
                    ret = elite_get_soundfocus_metadata(&sound_focus_metadata, ctl_sf);
                    if (ret != 0) {
                        printf("failed to get soundfocus metadata %d", ret);
                        goto done;
                    }
                }

                if (is_source_track_get) {
                    ret = elite_get_sourcetrack_metadata((void *)&source_track_metadata, sizeof(struct source_track_meta) - sizeof(struct timespec), ctl_st);
                    if (ret != 0) {
                        printf("failed to get source track meta data %d", ret);
                        goto done;
                    }
                    clock_gettime(CLOCK_MONOTONIC, &ts);
                    source_track_metadata.ts = ts;
                }

                /* Print meta data */
                fprintf(log_file, "time stamp sec %ld msec %ld \n", (source_track_metadata.ts).tv_sec,
                                              (source_track_metadata.ts).tv_nsec / NSEC_MSEC_CONVERT);
                for (idx = 0; idx < NUM_SECTORS; idx++){
                    printf("vad[%d]=%d ",idx, source_track_metadata.vad[idx]);
                    if (idx < (NUM_SECTORS-1))
                        printf("doa_noise[%d]=%d \n",
                                idx, source_track_metadata.doa_noise[idx]);
                }

                fprintf(log_file, "doa_speech=%d\n",source_track_metadata.doa_speech);
                if (is_sound_focus_get || is_sound_focus_set) {
                    fprintf(log_file, "polar_activity:");
                    for (sect = 0; sect < NUM_SECTORS; sect++ ){
                        fprintf(log_file, "Sector No-%d:",sect + 1);
                        idx = sound_focus_metadata.start_angle[sect];
                        fprintf(log_file, "idx %d:",idx);
                        count = sound_focus_metadata.start_angle[(sect + 1)%NUM_SECTORS] -
                                    sound_focus_metadata.start_angle[sect];
                        fprintf(log_file, "count %d:",count);
                        if (count < 0)
                            count = count + TOTAL_DEGREES;
                        do {
                            fprintf(log_file, "%d ",
                                source_track_metadata.polar_activity[idx%TOTAL_DEGREES]);
                            count--;
                            idx++;
                        } while (count);
                    }
                }
                break;
            }
            default:
                printf("fluence version is not supported");
                break;
        }

        usleep(get_data_time_gap);
        if (get_data_iter > 0)
            get_data_iter--;
    }


done:
    mixer_close(stt_mixer);
    stt_mixer = NULL;

    if ((log_file != stdout) && (log_file != nullptr))
        fclose(log_file);

    return ret;
}

static int audioreach_stt_meta_extract(void)
{
    int idx, count, sect, ret = 0;
    struct timespec ts;

    struct source_track_meta_fnn source_track_metadata_fnn;

    /* Open mixer for snd card 0 */
    struct mixer *stt_mixer = NULL;
    struct mixer_ctl *ctl_st = NULL, *ctl_sf = NULL, *ctl_st_fnn = NULL;

    bool is_source_track_get = g_stt_meta.is_source_track_get;
    bool is_sound_focus_get = g_stt_meta.is_sound_focus_get;
    bool is_sound_focus_set = g_stt_meta.is_sound_focus_set;

    unsigned int card = g_stt_meta.card;
    unsigned int device = g_stt_meta.device;
    enum fluence_version fv = g_stt_meta.fv;

    int get_data_iter = g_stt_meta.get_data_iter;
    int get_data_time_gap = g_stt_meta.get_data_time_gap;

    FILE * log_file = g_stt_meta.log_file;

    memset(&source_track_metadata_fnn, 0xFF, sizeof(struct source_track_meta_fnn));

    if (is_sound_focus_get || is_sound_focus_set) {
        ret = -EINVAL;
        printf("get_sound_focus & set_sound_focus is not support %d", ret);
        goto done;
    }

    fprintf(log_file, "get_source_track meta data with gap of %d us \n", get_data_time_gap);

    while (get_data_iter != 0) {
        switch (fv) {
            case FV_13:
            {
                if (is_source_track_get) {
                    ret = audioreach_get_soundtrack_metadata(card, device, g_stt_meta.be_intf);
                    if (ret != 0) {
                        printf("failed to get source track meta data %d", ret);
                        goto done;
                    }
                }

                break;
            }
            case FV_11:
            default:
                ret = -EINVAL;
                printf("fluence version is not supported: %d\n", fv);
                goto done;
        }

        usleep(get_data_time_gap);
        if (get_data_iter > 0)
            get_data_iter--;
    }

done:
    mixer_close(stt_mixer);
    stt_mixer = NULL;

    if ((log_file != stdout) && (log_file != nullptr))
        fclose(log_file);

    return ret;

}

int main(int argc, char* argv[]) {
    int idx, count, sect, ret = 0;
    char *be_intf = "CODEC_DMA-LPAIF_RXTX-TX-3";
    FILE * log_file = NULL;
    const char *log_filename = NULL;
    char fluence_property[PROPERTY_VALUE_MAX];
    struct timespec ts;

    struct sound_focus_meta sound_focus_metadataset;
    struct source_track_meta source_track_metadata;
    struct sound_focus_meta sound_focus_metadata;
    struct source_track_meta_fnn source_track_metadata_fnn;

    struct option long_options[] = {
        {"meta-time-gap",       required_argument,    0, 'g'},    // time-gap between two meta data
        {"get-data-iterations", required_argument,    0, 'n'},    // number of meta data events
        {"source_track_data",   required_argument,    0, 't'},    // Extract Source track meta data
        {"sound_focus_data",    required_argument,    0, 'f'},    // Extract Sound focus meta data
        {"sound focus set",     required_argument,    0, 's'},   // Set Sound focus meta data
        {"audio be interface",  required_argument,    0, 'b'},   // update audio back end interface
        {"audio virtual pcm index",  required_argument,    0, 'p'},   // update virtual pcm index
        {"sound card index",  required_argument,    0, 'd'},   // sound card index
        {"log file_name",       required_argument,    0, 'l'},   // update log file name
        {"help",                no_argument,          0, 'h'}
    };

    int opt = 0;
    int option_index = 0;
    char *str;
    log_file = stdout;

    stt_meta_init();

    while ((opt = getopt_long(argc,
                              argv,
                              "-g:n:t:f:s:b:d:p:l:h:",
                              long_options,
                              &option_index)) != -1) {
        printf("for argument %c, value is %s\n", opt, optarg);

        switch (opt) {
        case 'g':
            g_stt_meta.get_data_time_gap = NSEC_MSEC_CONVERT * atoi(optarg);
            break;
        case 'n':
            g_stt_meta.get_data_iter = atoi(optarg);     /* -ve value for cont get data, +ve for iterations */
            break;
        case 't':
            g_stt_meta.is_source_track_get = atoi(optarg);
            break;
        case 'f':
            g_stt_meta.is_sound_focus_get = atoi(optarg);
            break;
        case 's':
            str = optarg;
            elite_updatestt_data(str, &g_stt_meta.sound_focus_metadataset);
            g_stt_meta.is_sound_focus_set = true;
            break;
        case 'b':
            be_intf = optarg;
            memcpy(g_stt_meta.be_intf, be_intf, strlen(be_intf));
            break;
        case 'd':
            g_stt_meta.card = atoi(optarg);
            break;
        case 'p':
            g_stt_meta.device = atoi(optarg);
            break;
        case 'l':
            log_filename = optarg;
            if (strcasecmp(log_filename, "stdout") &&
                strcasecmp(log_filename, "1") &&
                (log_file = fopen(log_filename,"wb")) == NULL) {
                fprintf(log_file, "Cannot open log file %s\n", log_filename);
                fprintf(stderr, "Cannot open log file %s\n", log_filename);
                /* continue to log to std out. */
                log_file = stdout;
            }
            g_stt_meta.log_file = log_file;
            break;
        case 'h':
            usage();
            return 0;
            break;
        default:
            usage();
            return 0;
        }
    }

    if ((g_stt_meta.card >= 100) || (g_stt_meta.device >= 100))
        g_stt_meta.is_audio_reach = true;

    printf("STT GET META on %s \n", g_stt_meta.is_audio_reach?"AudioReach arch":"Elite arch");

    property_get("ro.vendor.audio.sdk.fluencetype", fluence_property, NULL);

    if (property_get_bool("ro.vendor.audio.sdk.fluence.nn.enabled",false)) {
        if((!strncmp("fluencenn", fluence_property, sizeof("fluencenn"))) ||
                       (!strncmp("none", fluence_property, sizeof("none"))))
            g_stt_meta.fv = FV_13;
        else
            g_stt_meta.fv = FV_11;
    }
    else {
        g_stt_meta.fv = FV_11;
    }

    fprintf(log_file, "get_source_track meta data with fluence_version: %s \n", g_stt_meta.fv == FV_11 ? "FV_11":"FV_13");

    if (g_stt_meta.is_audio_reach) {
        ret = audioreach_stt_meta_extract();
        if (ret != 0) {
            printf("on audioreach arch, failed to get source track meta data %d", ret);
            goto done;
        }
    } else {
        ret = elite_stt_meta_extract();
        if (ret != 0) {
            printf("on elite arch, failed to get source track meta data %d", ret);
            goto done;
        }
    }

done:

    if ((log_file != stdout) && (log_file != nullptr))
        fclose(log_file);

    fprintf(log_file, "\nADL: BYE BYE\n");

    return ret;
}
