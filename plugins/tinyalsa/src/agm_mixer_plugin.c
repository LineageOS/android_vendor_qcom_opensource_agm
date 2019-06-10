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

/* agm_mixer.c all names (variable/functions) should have amp_ (Agm Mixer Plugin) */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <limits.h>
#include <linux/ioctl.h>

#include <sound/asound.h>

#include <tinyalsa/mixer_plugin.h>
#include <tinyalsa/asoundlib.h>

#include <agm_api.h>
#include <snd-card-def/snd-card-def.h>

#define ARRAY_SIZE(a)    \
    (sizeof(a) / sizeof(a[0]))

#define AMP_PRIV_GET_CTL_PTR(p, idx) \
    (p->ctls + idx)

#define AMP_PRIV_GET_CTL_NAME_PTR(p, idx) \
    (p->ctl_names[idx])

#define BE_CTL_NAME_EXTN_MEDIA_CONFIG 0
#define BE_CTL_NAME_EXTN_METADATA 1

/* strings should be at the index as per the #defines */
static char *amp_be_ctl_name_extn[] = {
    "rate ch fmt",
    "metadata",
};

#define PCM_CTL_NAME_EXTN_CONNECT 0
#define PCM_CTL_NAME_EXTN_DISCONNECT 1
#define PCM_CTL_NAME_EXTN_MTD_CONTROL 2
#define PCM_CTL_NAME_EXTN_METADATA 3

/* strings should be at the index as per the #defines */
static char *amp_pcm_ctl_name_extn[] = {
    "connect",
    "disconnect",
    "control",
    "metadata",
};

#define PCM_TX_CTL_NAME_EXTN_LOOPBACK 0
/* strings should be at the index as per the #defines */
static char *amp_pcm_tx_ctl_names[] = {
    "loopback",
};

struct amp_dev_info {
 //   char (*names) [AIF_NAME_MAX_LEN];
    char **names;
    int *idx_arr;
    int count;
    struct snd_value_enum dev_enum;
    enum direction dir;

    /*
     * Mixer ctl data cache for
     * "pcm<id> metadata_control"
     * Unused for BE devs
     */
    int *pcm_mtd_ctl;
};

struct amp_priv {
    unsigned int card;
    void *snd_def_node;
    void *card_node;

    struct aif_info *aif_list;

    struct amp_dev_info rx_be_devs;
    struct amp_dev_info tx_be_devs;
    struct amp_dev_info rx_pcm_devs;
    struct amp_dev_info tx_pcm_devs;

    struct snd_control *ctls;
    char (*ctl_names)[AIF_NAME_MAX_LEN + 16];
    int ctl_count;

    struct snd_value_enum tx_be_enum;
    struct snd_value_enum rx_be_enum;

    struct agm_media_config media_fmt;
};

static enum agm_pcm_format alsa_to_agm_fmt(int fmt)
{
    enum agm_pcm_format agm_pcm_fmt = AGM_PCM_FORMAT_INVALID;

    switch (fmt) {
    case SNDRV_PCM_FORMAT_S8:
        agm_pcm_fmt = AGM_PCM_FORMAT_S8;
        break;
    case SNDRV_PCM_FORMAT_S16_LE:
        agm_pcm_fmt = AGM_PCM_FORMAT_S16_LE;
        break;
    case SNDRV_PCM_FORMAT_S24_LE:
        agm_pcm_fmt = AGM_PCM_FORMAT_S24_LE;
        break;
    case SNDRV_PCM_FORMAT_S24_3LE:
        agm_pcm_fmt = AGM_PCM_FORMAT_S24_3LE;
        break;
    case SNDRV_PCM_FORMAT_S32_LE:
        agm_pcm_fmt = AGM_PCM_FORMAT_S32_LE;
        break;
    }

    return agm_pcm_fmt;
}

static struct amp_dev_info *amp_get_be_adi(struct amp_priv *amp_priv,
                enum direction dir)
{
    if (dir == RX)
        return &amp_priv->rx_be_devs;
    else if (dir == TX)
        return &amp_priv->tx_be_devs;

    return NULL;
}

static void amp_free_dev_info(struct amp_dev_info *adi)
{
    if (adi->names) {
        free(adi->names);
        adi->names = NULL;
    }

    if (adi->idx_arr) {
        free(adi->idx_arr);
        adi->idx_arr = NULL;
    }

    if (adi->pcm_mtd_ctl) {
        free(adi->pcm_mtd_ctl);
        adi->pcm_mtd_ctl = NULL;
    }

    adi->count = 0;
}

static void amp_free_be_dev_info(struct amp_priv *amp_priv)
{
    amp_free_dev_info(&amp_priv->rx_be_devs);
    amp_free_dev_info(&amp_priv->tx_be_devs);

    if (amp_priv->aif_list) {
        free(amp_priv->aif_list);
        amp_priv->aif_list = NULL;
    }
}

static void amp_free_pcm_dev_info(struct amp_priv *amp_priv)
{
    amp_free_dev_info(&amp_priv->rx_pcm_devs);
    amp_free_dev_info(&amp_priv->tx_pcm_devs);
}

static void amp_free_ctls(struct amp_priv *amp_priv)
{
    if (amp_priv->ctl_names) {
        free(amp_priv->ctl_names);
        amp_priv->ctl_names = NULL;
    }

    if (amp_priv->ctls) {
        free(amp_priv->ctls);
        amp_priv->ctls = NULL;
    }

    amp_priv->ctl_count = 0;
}

static void amp_copy_be_names_from_aif_list(struct aif_info *aif_list,
                size_t aif_cnt, struct amp_dev_info *adi, enum direction dir)
{
    struct aif_info *aif_info;
    int i, be_idx = 0;

    be_idx = 0;
    adi->names[be_idx] = "ZERO";
    adi->idx_arr[be_idx] = 0;
    be_idx++;

    for (i = 0; i < aif_cnt; i++) {
        aif_info = aif_list + i;
        if (aif_info->dir != dir)
            continue;

        adi->names[be_idx] = aif_info->aif_name;
        adi->idx_arr[be_idx] = i;
        be_idx++;
    }

    adi->count = be_idx;
    adi->dev_enum.items = adi->count;
    adi->dev_enum.texts = adi->names;
}

static int amp_get_be_info(struct amp_priv *amp_priv)
{
    struct amp_dev_info *rx_adi = &amp_priv->rx_be_devs;
    struct amp_dev_info *tx_adi = &amp_priv->tx_be_devs;
    struct aif_info *aif_list, *aif_info;
    size_t be_count = 0;
    int ret = 0, i;

    ret = agm_get_aif_info_list(NULL, &be_count);
    if (ret || be_count == 0)
        return -EINVAL;

    aif_list = calloc(be_count, sizeof(struct aif_info));
    if (!aif_list)
        return -ENOMEM;

    ret = agm_get_aif_info_list(aif_list, &be_count);
    if (ret)
        goto err_backends_get;

    rx_adi->count = 0;
    tx_adi->count = 0;

    /* count rx and tx backends */
    for (i = 0; i < be_count; i++) {
        aif_info = aif_list + i;
        if (aif_info->dir == RX)
            rx_adi->count++;
        else if (aif_info->dir == TX)
            tx_adi->count++;
    }

    rx_adi->names = calloc(rx_adi->count + 1, sizeof(*rx_adi->names));
    rx_adi->idx_arr = calloc(rx_adi->count + 1, sizeof(*rx_adi->idx_arr));
    tx_adi->names = calloc(tx_adi->count + 1, sizeof(*tx_adi->names));
    tx_adi->idx_arr = calloc(tx_adi->count + 1, sizeof(*tx_adi->idx_arr));

    if (!rx_adi->names || !tx_adi->names ||
        !rx_adi->idx_arr || !tx_adi->idx_arr) {
        ret = -ENOMEM;
        goto err_backends_get;
    }

    /* form the rx backends enum array */
    amp_copy_be_names_from_aif_list(aif_list, be_count, rx_adi, RX);
    amp_copy_be_names_from_aif_list(aif_list, be_count, tx_adi, TX);

    amp_priv->aif_list = aif_list;
    return 0;

err_backends_get:
    amp_free_be_dev_info(amp_priv);
    return ret;
}

static int amp_create_pcm_info_from_card(struct amp_dev_info *adi,
            const char *dir, int num_pcms, void **pcm_node_list)
{
    int ret, i, val = 0, idx = 0;

    adi->names[idx] =  "ZERO";
    adi->idx_arr[idx] = 0;
    idx++;

    for (i = 0; i < num_pcms; i++) {
        void *pcm_node = pcm_node_list[i];
        snd_card_def_get_int(pcm_node, dir, &val);
        if (val == 0)
            continue;

        ret = snd_card_def_get_str(pcm_node, "name",
                                   &adi->names[idx]);
        if (ret) {
            printf("%s failed to get name for %s pcm wih idx %d\n",
                   __func__, dir, idx);
            return -EINVAL;
        }
        ret = snd_card_def_get_int(pcm_node, "id",
                                   &adi->idx_arr[idx]);
        if (ret) {
            printf("%s failed to get name for %s pcm with idx %d\n",
                   __func__, dir, idx);
            return -EINVAL;
        }

        idx++;
    }

    adi->count = idx;

    return 0;
}

static int amp_get_pcm_info(struct amp_priv *amp_priv)
{
    struct amp_dev_info *rx_adi = &amp_priv->rx_pcm_devs;
    struct amp_dev_info *tx_adi = &amp_priv->tx_pcm_devs;
    void **pcm_node_list = NULL;
    int num_pcms, ret, val = 0, i;

    num_pcms = snd_card_def_get_num_node(amp_priv->card_node,
                                         SND_NODE_TYPE_PCM);
    if (num_pcms <= 0) {
        printf("%s: no pcms found for card %u\n",
               __func__, amp_priv->card);
        ret = -EINVAL;
        goto done;
    }

    pcm_node_list = calloc(num_pcms, sizeof(*pcm_node_list));
    if (!pcm_node_list) {
        printf("%s: alloc for pcm_node_list failed\n", __func__);
        return -ENOMEM;
    }

    ret = snd_card_def_get_nodes_for_type(amp_priv->card_node,
                                          SND_NODE_TYPE_PCM,
                                          pcm_node_list, num_pcms);
    if (ret) {
        printf("%s: failed to get pcm node list, err %d\n",
               __func__, ret);
        goto done;
    }

    /* count TX and RX PCMs */
    for (i = 0; i < num_pcms; i++) {
        void *pcm_node = pcm_node_list[i];
        snd_card_def_get_int(pcm_node, "playback", &val);
        if (val == 1)
            rx_adi->count++;
    }
    val = 0;
    for (i = 0; i < num_pcms; i++) {
        void *pcm_node = pcm_node_list[i];
        snd_card_def_get_int(pcm_node, "capture", &val);
        if (val == 1)
            tx_adi->count++;
    }

    /* Allocate rx and tx structures */
    rx_adi->names = calloc(rx_adi->count + 1, sizeof(*rx_adi->names));
    rx_adi->idx_arr = calloc(rx_adi->count + 1, sizeof(*rx_adi->idx_arr));
    tx_adi->names = calloc(tx_adi->count + 1, sizeof(*tx_adi->names));
    tx_adi->idx_arr = calloc(tx_adi->count + 1, sizeof(*tx_adi->idx_arr));

    if (!rx_adi->names || !tx_adi->names ||
        !rx_adi->idx_arr || !tx_adi->idx_arr) {
        ret = -ENOMEM;
        goto err_alloc_rx_tx;
    }
    
    
    /* Fill in RX properties */
    ret = amp_create_pcm_info_from_card(rx_adi, "playback",
                                        num_pcms, pcm_node_list);
    if (ret)
        goto err_alloc_rx_tx;

    ret = amp_create_pcm_info_from_card(tx_adi, "capture",
                                        num_pcms, pcm_node_list);
    if (ret)
        goto err_alloc_rx_tx;

    rx_adi->dev_enum.items = rx_adi->count;
    rx_adi->dev_enum.texts = rx_adi->names;
    tx_adi->dev_enum.items = tx_adi->count;
    tx_adi->dev_enum.texts = tx_adi->names;

    goto done;

err_alloc_rx_tx:
    amp_free_pcm_dev_info(amp_priv);
    
done:
    if (pcm_node_list)
        free(pcm_node_list);

    return ret;
}

static int amp_get_be_ctl_count(struct amp_priv *amp_priv)
{
    struct amp_dev_info *rx_adi = &amp_priv->rx_be_devs;
    struct amp_dev_info *tx_adi = &amp_priv->tx_be_devs;
    int count, ctl_per_be;

    ctl_per_be = ARRAY_SIZE(amp_be_ctl_name_extn);

    count = 0;

    /* minus 1 is needed to ignore the ZERO string (name) */
    count += (rx_adi->count - 1) * ctl_per_be;
    count += (tx_adi->count - 1) * ctl_per_be;

    return count;
}

static int amp_get_pcm_ctl_count(struct amp_priv *amp_priv)
{
    struct amp_dev_info *rx_adi = &amp_priv->rx_pcm_devs;
    struct amp_dev_info *tx_adi = &amp_priv->tx_pcm_devs;
    int count, ctl_per_pcm;

    count = 0;

    /* Count common ctls applicable for both RX and TX pcms */
    ctl_per_pcm = ARRAY_SIZE(amp_pcm_ctl_name_extn);
    count += (rx_adi->count - 1) * ctl_per_pcm;
    count += (tx_adi->count - 1) * ctl_per_pcm;

    /* Count only TX pcm specific controls */
    ctl_per_pcm = ARRAY_SIZE(amp_pcm_tx_ctl_names);
    count += (tx_adi->count -1) * ctl_per_pcm;

    return count;
}

/* 512 max bytes for non-tlv controls, reserving 16 for future use */
static struct snd_value_bytes be_metadata_bytes =
    SND_VALUE_BYTES(512 - 16);
static struct snd_value_bytes pcm_metadata_bytes =
    SND_VALUE_BYTES(512 - 16);

static struct snd_value_int media_fmt_int = 
    SND_VALUE_INTEGER(3, 0, 384000, 1);

static int amp_be_media_fmt_get(struct mixer_plugin *plugin,
                struct snd_control *ctl, struct snd_ctl_elem_value *ev)
{
    //TODO: AGM should support get function.
    printf ("%s: enter\n", __func__);
    return 0;
}

static int amp_be_media_fmt_put(struct mixer_plugin *plugin,
                struct snd_control *ctl, struct snd_ctl_elem_value *ev)
{
    struct amp_priv *amp_priv = plugin->priv;
    uint32_t audio_intf_id = ctl->private_value;
    int ret = 0;

    printf ("%s: enter\n", __func__);
    amp_priv->media_fmt.rate = ev->value.integer.value[0];
    amp_priv->media_fmt.channels = ev->value.integer.value[1];
    amp_priv->media_fmt.format = alsa_to_agm_fmt(ev->value.integer.value[2]);

    ret = agm_audio_intf_set_media_config(audio_intf_id,
                                          &amp_priv->media_fmt);
    if (ret)
        printf("%s: set_media_config failed, err %d, aif_id %u rate %u channels %u fmt %u\n",
               __func__, ret, audio_intf_id, amp_priv->media_fmt.rate, 
               amp_priv->media_fmt.channels, amp_priv->media_fmt.format);
    return ret;
}

static int amp_be_metadata_get(struct mixer_plugin *plugin,
                struct snd_control *ctl, struct snd_ctl_elem_value *ev)
{
    /* AGM should provide a get */
    printf ("%s: enter\n", __func__);
    return 0;
}

static int amp_be_metadata_put(struct mixer_plugin *plugin,
                struct snd_control *ctl, struct snd_ctl_elem_value *ev)
{
    uint32_t audio_intf_id = ctl->private_value;
    struct agm_meta_data *metadata;
    int ret;

       metadata = (struct agm_meta_data *) ev->value.bytes.data;
    printf ("%s: enter\n", __func__);
    ret = agm_audio_intf_set_metadata(audio_intf_id, metadata);
    if (ret)
        printf("%s: set_metadata failed, err %d, aid_id %u\n",
               __func__, ret, audio_intf_id);
    return ret;
}

static int amp_pcm_aif_connect_get(struct mixer_plugin *plugin,
                struct snd_control *ctl, struct snd_ctl_elem_value *ev)
{
    /* TODO: Need AGM support to perform get */
    printf("%s: enter\n", __func__);
    return 0;
}

static int amp_pcm_aif_connect_put(struct mixer_plugin *plugin,
                struct snd_control *ctl, struct snd_ctl_elem_value *ev)
{
    struct amp_priv *amp_priv = plugin->priv;
    struct amp_dev_info *pcm_adi = ctl->private_data;
    struct amp_dev_info *be_adi;
    int be_idx, pcm_idx = ctl->private_value;
    unsigned int val;
    int ret;
    bool state;

    printf ("%s: enter\n", __func__);
    be_adi = amp_get_be_adi(amp_priv, pcm_adi->dir);
    if (!be_adi) 
        return -EINVAL;

    val = ev->value.enumerated.item[0];

    /* setting to ZERO is a no-op */
    if (val == 0)
        return 0;

    /*
     * same function caters to connect and disconnect mixer ctl.
     * try to find "disconnect" in the ctl name to differentiate
     * between connect and disconnect mixer ctl.
     */
    if (strstr(ctl->name, "disconnect"))
        state = false;
    else
        state = true;

    be_idx = be_adi->idx_arr[val];
    ret = agm_session_audio_inf_connect(pcm_idx, be_idx, state);
    if (ret)
        printf("%s: connect failed err %d, pcm_idx %d be_idx %d\n",
               __func__, ret, pcm_idx, be_idx);

    return 0;
}

static int amp_pcm_mtd_control_get(struct mixer_plugin *plugin,
                struct snd_control *ctl, struct snd_ctl_elem_value *ev)
{
    struct amp_dev_info *pcm_adi = ctl->private_data;
    int idx    = ctl->private_value;

    ev->value.enumerated.item[0] = pcm_adi->pcm_mtd_ctl[idx];

    printf ("%s: enter, val = %u\n", __func__,
            ev->value.enumerated.item[0]);
    return 0;
}

static int amp_pcm_mtd_control_put(struct mixer_plugin *plugin,
               struct snd_control *ctl, struct snd_ctl_elem_value *ev)
{
    struct amp_dev_info *pcm_adi = ctl->private_data;
    int idx = ctl->private_value;
    unsigned int val;

    val = ev->value.enumerated.item[0];
    pcm_adi->pcm_mtd_ctl[idx] = val;

    printf("%s: value = %u\n", __func__, val);
    return 0;
}

static int amp_pcm_metadata_get(struct mixer_plugin *plugin,
                struct snd_control *Ctl, struct snd_ctl_elem_value *ev)
{
    /* TODO: AGM needs to provide this in a API */
    printf ("%s: enter\n", __func__);
    return 0;
}

static int amp_pcm_metadata_put(struct mixer_plugin *plugin,
                struct snd_control *ctl, struct snd_ctl_elem_value *ev)
{
    struct amp_dev_info *pcm_adi = ctl->private_data;
    struct amp_dev_info *be_adi;
    struct agm_meta_data *metadata;
    int pcm_idx = ctl->private_value;
    int mtd_control, be_idx, ret, mtd_idx;

    printf("%s: enter\n", __func__);

    metadata = (struct agm_meta_data *) &ev->value.bytes.data[0];
    /* Find the index for metadata_ctl for this pcm */
    for (mtd_idx = 1; mtd_idx < pcm_adi->count; mtd_idx++) {
        if (pcm_idx == pcm_adi->idx_arr[mtd_idx])
            break;
    }

    if (mtd_idx >= pcm_adi->count) {
        printf("%s: metadata index not found for ctl %s",
               __func__, ctl->name);
        return -EINVAL;
    }

    mtd_control = pcm_adi->pcm_mtd_ctl[mtd_idx];
    if (mtd_control == 0) {
        ret = agm_session_set_metadata(pcm_idx, metadata);
        if (ret)
            printf("%s: set_session_metadata failed err %d for %s\n",
                   __func__, ret, ctl->name);
        return ret;
    } 
    
    /* metadata control is not 0, set the (session + be) metadata */
    be_adi = amp_get_be_adi(plugin->priv, pcm_adi->dir);
    be_idx = be_adi->idx_arr[mtd_control];
    ret = agm_session_audio_inf_set_metadata(pcm_idx, be_idx, metadata);
    if (ret)
        printf("%s: set_aif_ses_metadata failed err %d for %s\n",
               __func__, ret, ctl->name);
    return ret;
}

static int amp_pcm_loopback_get(struct mixer_plugin *plugin,
                struct snd_control *Ctl, struct snd_ctl_elem_value *ev)
{
    /* TODO: AGM API not available */
    printf ("%s: enter\n", __func__);
    return 0;
}

static int amp_pcm_loopback_put(struct mixer_plugin *plugin,
                struct snd_control *ctl, struct snd_ctl_elem_value *ev)
{
    struct amp_priv *amp_priv = plugin->priv;
    struct amp_dev_info *pcm_tx_adi = ctl->private_data;
    struct amp_dev_info *pcm_rx_adi;
    int rx_pcm_idx, tx_pcm_idx = ctl->private_value;
    unsigned int val;
    bool state = true;
    int ret;

    printf ("%s: enter\n", __func__);
    pcm_rx_adi = &amp_priv->rx_pcm_devs;
    if (!pcm_rx_adi)
        return -EINVAL;

    val = ev->value.enumerated.item[0];

    /* setting to ZERO is a no-op */
    if (val == 0) {
        rx_pcm_idx = 0;
        state = false;
    } else {
        rx_pcm_idx = pcm_rx_adi->idx_arr[val];
    }

    ret = agm_session_set_loopback(tx_pcm_idx, rx_pcm_idx, state);
    if (ret)
        printf("%s: loopback failed err %d, tx_pcm_idx %d rx_pcm_idx %d\n",
               __func__, ret, tx_pcm_idx, rx_pcm_idx);

    return 0;
}

/* PCM related mixer controls here */
static void amp_create_connect_ctl(struct amp_priv *amp_priv,
            char *pname, int ctl_idx, struct snd_value_enum *e,
            int pval, void *pdata)
{
    struct snd_control *ctl = AMP_PRIV_GET_CTL_PTR(amp_priv, ctl_idx);
    char *ctl_name = AMP_PRIV_GET_CTL_NAME_PTR(amp_priv, ctl_idx);

    snprintf(ctl_name, AIF_NAME_MAX_LEN + 16, "%s %s",
             pname, amp_pcm_ctl_name_extn[PCM_CTL_NAME_EXTN_CONNECT]);
    printf("%s - %s\n", __func__, ctl_name);
    INIT_SND_CONTROL_ENUM(ctl, ctl_name, amp_pcm_aif_connect_get,
                    amp_pcm_aif_connect_put, e, pval, pdata);
}

static void amp_create_disconnect_ctl(struct amp_priv *amp_priv,
            char *pname, int ctl_idx, struct snd_value_enum *e,
            int pval, void *pdata)
{
    struct snd_control *ctl = AMP_PRIV_GET_CTL_PTR(amp_priv, ctl_idx);
    char *ctl_name = AMP_PRIV_GET_CTL_NAME_PTR(amp_priv, ctl_idx);

    snprintf(ctl_name, AIF_NAME_MAX_LEN + 16, "%s %s",
             pname, amp_pcm_ctl_name_extn[PCM_CTL_NAME_EXTN_DISCONNECT]);
    printf("%s - %s\n", __func__, ctl_name);
    INIT_SND_CONTROL_ENUM(ctl, ctl_name, amp_pcm_aif_connect_get,
                    amp_pcm_aif_connect_put, e, pval, pdata);
}

static void amp_create_mtd_control_ctl(struct amp_priv *amp_priv,
                char *pname, int ctl_idx, struct snd_value_enum *e,
                int pval, void *pdata)
{
    struct snd_control *ctl = AMP_PRIV_GET_CTL_PTR(amp_priv, ctl_idx);
    char *ctl_name = AMP_PRIV_GET_CTL_NAME_PTR(amp_priv, ctl_idx);

    snprintf(ctl_name, AIF_NAME_MAX_LEN + 16, "%s %s",
             pname, amp_pcm_ctl_name_extn[PCM_CTL_NAME_EXTN_MTD_CONTROL]);
    printf("%s - %s\n", __func__, ctl_name);
    INIT_SND_CONTROL_ENUM(ctl, ctl_name, amp_pcm_mtd_control_get,
                    amp_pcm_mtd_control_put, e, pval, pdata);

}

static void amp_create_pcm_metadata_ctl(struct amp_priv *amp_priv,
                char *name, int ctl_idx, int pval, void *pdata)
{
    struct snd_control *ctl = AMP_PRIV_GET_CTL_PTR(amp_priv, ctl_idx);
    char *ctl_name = AMP_PRIV_GET_CTL_NAME_PTR(amp_priv, ctl_idx);

    snprintf(ctl_name, AIF_NAME_MAX_LEN + 16, "%s %s",
             name, amp_pcm_ctl_name_extn[PCM_CTL_NAME_EXTN_METADATA]);

    printf("%s - %s\n", __func__, ctl_name);
    INIT_SND_CONTROL_BYTES(ctl, ctl_name, amp_pcm_metadata_get,
                    amp_pcm_metadata_put, pcm_metadata_bytes,
                    pval, pdata);
}

static void amp_create_pcm_loopback_ctl(struct amp_priv *amp_priv,
            char *pname, int ctl_idx, struct snd_value_enum *e,
            int pval, void *pdata)
{
    struct snd_control *ctl = AMP_PRIV_GET_CTL_PTR(amp_priv, ctl_idx);
    char *ctl_name = AMP_PRIV_GET_CTL_NAME_PTR(amp_priv, ctl_idx);

    snprintf(ctl_name, AIF_NAME_MAX_LEN + 16, "%s %s",
             pname, amp_pcm_tx_ctl_names[PCM_TX_CTL_NAME_EXTN_LOOPBACK]);
    printf("%s - %s\n", __func__, ctl_name);
    INIT_SND_CONTROL_ENUM(ctl, ctl_name, amp_pcm_loopback_get,
                    amp_pcm_loopback_put, e, pval, pdata);
}


/* BE related mixer control creations here */
static void amp_create_metadata_ctl(struct amp_priv *amp_priv,
                char *be_name, int ctl_idx, int pval, void *pdata)
{
    struct snd_control *ctl = AMP_PRIV_GET_CTL_PTR(amp_priv, ctl_idx);
    char *ctl_name = AMP_PRIV_GET_CTL_NAME_PTR(amp_priv, ctl_idx);

    snprintf(ctl_name, AIF_NAME_MAX_LEN + 16, "%s %s",
             be_name, amp_be_ctl_name_extn[BE_CTL_NAME_EXTN_METADATA]);

    printf("%s - %s\n", __func__, ctl_name);
    INIT_SND_CONTROL_BYTES(ctl, ctl_name, amp_be_metadata_get,
                    amp_be_metadata_put, be_metadata_bytes,
                    pval, pdata);
}

static void amp_create_media_fmt_ctl(struct amp_priv *amp_priv,
                char *be_name, int ctl_idx, int pval, void *pdata)
{
    struct snd_control *ctl = AMP_PRIV_GET_CTL_PTR(amp_priv, ctl_idx);
    char *ctl_name = AMP_PRIV_GET_CTL_NAME_PTR(amp_priv, ctl_idx);

    snprintf(ctl_name, AIF_NAME_MAX_LEN + 16, "%s %s",
             be_name, amp_be_ctl_name_extn[BE_CTL_NAME_EXTN_MEDIA_CONFIG]);
    printf("%s - %s\n", __func__, ctl_name);
    INIT_SND_CONTROL_INTEGER(ctl, ctl_name, amp_be_media_fmt_get,
                    amp_be_media_fmt_put, media_fmt_int, pval, pdata);
}

static int amp_form_be_ctls(struct amp_priv *amp_priv, int ctl_idx, int ctl_cnt)
{
    struct amp_dev_info *rx_adi = &amp_priv->rx_be_devs;
    struct amp_dev_info *tx_adi = &amp_priv->tx_be_devs;
    int i;

    for (i = 1; i < rx_adi->count; i++) {
        amp_create_media_fmt_ctl(amp_priv, rx_adi->names[i], ctl_idx,
                                 rx_adi->idx_arr[i], rx_adi);
        ctl_idx++;
        amp_create_metadata_ctl(amp_priv, rx_adi->names[i], ctl_idx,
                                rx_adi->idx_arr[i], rx_adi);
        ctl_idx++;
    }

    for (i = 1; i < tx_adi->count; i++) {
        amp_create_media_fmt_ctl(amp_priv, tx_adi->names[i], ctl_idx,
                                 tx_adi->idx_arr[i], tx_adi);
        ctl_idx++;
        amp_create_metadata_ctl(amp_priv, tx_adi->names[i], ctl_idx,
                                tx_adi->idx_arr[i], tx_adi);
        ctl_idx++;
    }

    return 0;
}

static int amp_form_pcm_ctls(struct amp_priv *amp_priv, int ctl_idx, int ctl_cnt)
{
    struct amp_dev_info *rx_adi = &amp_priv->rx_pcm_devs;
    struct amp_dev_info *tx_adi = &amp_priv->tx_pcm_devs;
    struct amp_dev_info *be_rx_adi = &amp_priv->rx_be_devs;
    struct amp_dev_info *be_tx_adi = &amp_priv->tx_be_devs;
    int i;

    for (i = 1; i < rx_adi->count; i++) {
        char *name = rx_adi->names[i];
        int idx = rx_adi->idx_arr[i];
        amp_create_connect_ctl(amp_priv, name, ctl_idx,
                        &be_rx_adi->dev_enum, idx, rx_adi);
        ctl_idx++;
        amp_create_disconnect_ctl(amp_priv, name, ctl_idx,
                        &be_rx_adi->dev_enum, idx, rx_adi);
        ctl_idx++;

        rx_adi->pcm_mtd_ctl = calloc(rx_adi->count, sizeof(int));
        if (!rx_adi->pcm_mtd_ctl)
            return -ENOMEM;

        amp_create_mtd_control_ctl(amp_priv, name, ctl_idx,
                        &be_rx_adi->dev_enum, i, rx_adi);
        ctl_idx++;
        amp_create_pcm_metadata_ctl(amp_priv, name, ctl_idx, idx, rx_adi);
        ctl_idx++;
    }

    for (i = 1; i < tx_adi->count; i++) {
        char *name = tx_adi->names[i];
        int idx = tx_adi->idx_arr[i];
        amp_create_connect_ctl(amp_priv, name, ctl_idx,
                        &be_tx_adi->dev_enum, idx, tx_adi);
        ctl_idx++;
        amp_create_disconnect_ctl(amp_priv, name, ctl_idx,
                        &be_tx_adi->dev_enum, idx, tx_adi);
        ctl_idx++;

        tx_adi->pcm_mtd_ctl = calloc(tx_adi->count, sizeof(int));
        if (!tx_adi->pcm_mtd_ctl)
            return -ENOMEM;
        amp_create_mtd_control_ctl(amp_priv, name, ctl_idx,
                        &be_tx_adi->dev_enum, i, tx_adi);
        ctl_idx++;
        amp_create_pcm_metadata_ctl(amp_priv, name, ctl_idx, idx, tx_adi);
        ctl_idx++;
        /* create loopback control for TX PCMs, enum values are RX PCMs*/
        amp_create_pcm_loopback_ctl(amp_priv, name, ctl_idx,
                        &rx_adi->dev_enum, idx, tx_adi);
        ctl_idx++;
    }

    return 0;
}

static int amp_subscribe_events(struct mixer_plugin *plugin,
                int *subscribe)
{
    struct amp_priv *amp_priv = plugin->priv;

    printf("%s: enter, card: %d\n", __func__, amp_priv->card);
    return 0;
}

static void amp_close(struct mixer_plugin **plugin)
{
    struct mixer_plugin *amp = *plugin;
    struct amp_priv *amp_priv = amp->priv;

    snd_card_def_put_card(amp_priv->card_node);
    amp_free_pcm_dev_info(amp_priv);
    amp_free_be_dev_info(amp_priv);
    amp_free_ctls(amp_priv);
    free(amp_priv);
    free(*plugin);

    plugin = NULL;
}

struct mixer_plugin_ops amp_ops = {
    .close = amp_close,
    .subscribe_events = amp_subscribe_events,
};

MIXER_PLUGIN_OPEN_FN(agm_mixer)
{
    struct mixer_plugin *amp;
    struct amp_priv *amp_priv;
    struct pcm_adi;
    int ret = 0;
    int be_ctl_cnt, pcm_ctl_cnt, total_ctl_cnt = 0;

    printf("%s: enter, card %u\n", __func__, card);

    amp = calloc(1, sizeof(*amp));
    if (!amp) {
        printf("agm mixer plugin alloc failed\n");
        return -ENOMEM;
    }

    amp_priv = calloc(1, sizeof(*amp_priv));
    if (!amp_priv) {
        printf("amp priv data alloc failed\n");
        ret = -ENOMEM;
        goto err_priv_alloc;
    }

    amp_priv->card = card;
    amp_priv->card_node = snd_card_def_get_card(amp_priv->card);
    if (!amp_priv->card_node) {
        printf("%s: card node not found for card %d\n",
               __func__, amp_priv->card);
        ret = -EINVAL;
        goto err_get_card;
    }

    amp_priv->snd_def_node = snd_def_node;

    amp_priv->rx_be_devs.dir = RX;
    amp_priv->tx_be_devs.dir = TX;
    amp_priv->rx_pcm_devs.dir = RX;
    amp_priv->tx_pcm_devs.dir = TX;

    ret = amp_get_be_info(amp_priv);
    if (ret)
        goto err_get_be_info;
    ret = amp_get_pcm_info(amp_priv);
    if (ret)
        goto err_get_pcm_info;

    /* Get total count of controls to be registered */
    be_ctl_cnt = amp_get_be_ctl_count(amp_priv);
    total_ctl_cnt += be_ctl_cnt;
    pcm_ctl_cnt = amp_get_pcm_ctl_count(amp_priv);
    total_ctl_cnt += pcm_ctl_cnt;

    /*
     * Create the controls to be registered
     * When changing this code, be careful to make sure to create
     * exactly the same number of controls as of total_ctl_cnt;
     */
    amp_priv->ctls = calloc(total_ctl_cnt, sizeof(*amp_priv->ctls));
    amp_priv->ctl_names = calloc(total_ctl_cnt, sizeof(*amp_priv->ctl_names));
    if (!amp_priv->ctls || !amp_priv->ctl_names)
            goto err_ctls_alloc;

    ret = amp_form_be_ctls(amp_priv, 0, be_ctl_cnt);
    if (ret)
        goto err_ctls_alloc;
    ret = amp_form_pcm_ctls(amp_priv, be_ctl_cnt, pcm_ctl_cnt);
    if (ret)
        goto err_ctls_alloc;

    /* Register the controls */
    if (total_ctl_cnt > 0) {
        amp_priv->ctl_count = total_ctl_cnt;
        amp->controls = amp_priv->ctls;
        amp->num_controls = amp_priv->ctl_count;
    }

    amp->ops = &amp_ops;
    amp->priv = amp_priv;
    *plugin = amp;

    printf("%s: total_ctl_cnt = %d\n", __func__, total_ctl_cnt);

    return 0;

err_ctls_alloc:
    amp_free_ctls(amp_priv);
    amp_free_pcm_dev_info(amp_priv);

err_get_pcm_info:
    amp_free_be_dev_info(amp_priv);

err_get_be_info:
    snd_card_def_put_card(amp_priv->card_node);

err_get_card:
    free(amp_priv);

err_priv_alloc:
    free(amp);
    return -ENOMEM;
}
