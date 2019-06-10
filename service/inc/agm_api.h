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

#ifndef _AGM_INTF_H_
#define _AGM_INTF_H_
/**
 *=============================================================================
 * \file agm_api.h
 *
 * \brief
 *      Defines public APIs for Audio Graph Manager (AGM)
 *=============================================================================
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

struct session_obj;

/**
 * A single entry of a Key Vector
 */
struct agm_key_value {
    uint32_t key; /**< key */
    uint32_t value; /**< value */
};

/**
 * Key Vector
 */
struct agm_key_vector {
    size_t num_kvs;                 /**< number of key value pairs */
    struct agm_key_value *kv;       /**< array of key value pairs */
};

/**
 * Metadata Key Vectors
 */
struct agm_meta_data {
    /**
    * Used to lookup the calibration data
    */
    struct agm_key_vector gkv;

    /**
    * Used to lookup the calibration data
    */
     struct agm_key_vector ckv;
};

/**
 * Bit formats
 */
enum agm_pcm_format {
	AGM_PCM_FORMAT_INVALID,
	AGM_PCM_FORMAT_S8,          /**< 8-bit signed */
	AGM_PCM_FORMAT_S16_LE,      /**< 16-bit signed */
	AGM_PCM_FORMAT_S24_LE,      /**< 24-bits in 4-bytes */
	AGM_PCM_FORMAT_S24_3LE,     /**< 24-bits in 3-bytes */
	AGM_PCM_FORMAT_S32_LE,      /**< 32-bit signed */
	AGM_PCM_FORMAT_MAX,
};

/**
 * Media Config
 */
struct agm_media_config {
    uint32_t rate;                 /**< sample rate */
    uint32_t channels;             /**< number of channels */
    enum agm_pcm_format format;    /**< format */
};

/**
 * Session Direction
 */
enum direction {
	RX = 1, /**< RX */
	TX,     /**< TX */
};

/**
 * MAX length of the AIF Name
 */
#define AIF_NAME_MAX_LEN 28

/**
 * AIF Info
 */
struct aif_info {
	char aif_name[AIF_NAME_MAX_LEN];  /**< AIF name  */
	enum direction dir;               /**< direction */
};

/**
 * Session Config
 */
struct agm_session_config {
    enum direction dir;        /**< TX or RX */
    bool is_hostless;          /**< no data exchange on this session, e.g loopback */
    bool is_pcm;               /**< data stream is pcm or compressed */
    uint32_t start_threshold;  /**< start_threshold: number of buffers * buffer size */
    uint32_t stop_threshold;   /**< stop_th6reshold: number of buffers * buffer size */
};

/**
 * Buffer Config
 */
struct agm_buffer_config {
    uint32_t count; /**< number of buffers */
    size_t size;   /**< size of each buffer */
};

/**
 *  \brief Initialize agm.
 *
 *  \return 0 on success, error code on failure.
 */
int agm_init( );

/**
 *  \brief De-Initialize agm.
 *
 *  \return 0 on success, error code on failure.
 */
int agm_deinit();

 /**
  * \brief Set media configuration for an audio interface.
  *
  * \param[in] audio_intf - Valid audio interface id
  * \param[in] media_config - valid media configuration for the
  *       audio interafce.
  *
  *  \return 0 on success, error code on failure.
  *       If the audio interface is already in use and the
  *       new media_config is different from previous, api will return
  *       failure.
  */
int agm_audio_intf_set_media_config(uint32_t audio_intf,
                                    struct agm_media_config *media_config);


 /**
  * \brief Set metadata for an audio interface.
  *
  * \param[in] audio_intf - Valid audio interface id
  * \param[in] metadata - valid metadata for the audio
  *       interafce.
  *
  *  \return 0 on success, error code on failure.
  *       If the audio interface is already in use and the
  *       new meta data is set, api will return
  *       failure.
  */
int agm_audio_intf_set_metadata(uint32_t audio_intf,
                                struct agm_meta_data *metadata);

 /**
  * \brief Set metadata for the session.
  *
  * \param[in] session_id - Valid audio session id
  * \param[in] metadata - valid metadata for the session.
  *
  *  \return 0 on success, error code on failure.
  *       If the session is already opened and the new
  *       meta data is set, api will return failure.
  */
int agm_session_set_metadata(uint32_t session_id,
                                struct agm_meta_data *metadata);

 /**
  * \brief Set metadata for the session, audio intf pair.
  *
  * \param[in] session_id - Valid audio session id
  * \param[in] audio_intf - Valid audio interface id
  * \param[in] metadata - valid metadata for the session and
  *            audio_intf.
  *
  *  \return 0 on success, error code on failure.
  *       If the session is already opened and the new
  *       meta data is set, api will return failure.
  */
int agm_session_audio_inf_set_metadata(uint32_t session_id,
                                           uint32_t audio_intf,
                                           struct agm_meta_data *metadata);

/**
 * \brief Set metadata for the session, audio intf pair.
 *
 * \param[in] session_id - Valid audio session id
 * \param[in] audio_intf - Valid audio interface id
 * \param[in] state - Connect or Disconnect AIF to Session
 *
 *  \return 0 on success, error code on failure.
 *       If the session is already opened and the new
 *       meta data is set, api will return failure.
 */
int agm_session_audio_inf_connect(uint32_t session_id,
	uint32_t audio_intf,
	bool state);

/**
  * \brief Open the session with specified session id.
  *
 * \param[in] session_id - Valid audio session id
 * \param[out] handle - updated with valid session
  *       handle if the operation is successful.
  *
  * \return 0 on success, error code otherwise
  */
int agm_session_open(uint32_t session_id, struct session_obj **handle);

/**
  * \brief Set Session config
  *
  * \param[in] handle - Valid session handle obtained
  *       from agm_session_open
  * \param[in] session_config - valid stream configuration of the
  *       sessions
  * \param[in] media_config - valid media configuration of the
  *       session.
  * \param[in] buffer_config - buffer configuration for the
  *       session. Null if hostless
  *
  * \return 0 on success, error code otherwise
  */
int agm_session_set_config(struct session_obj *handle,
	struct agm_session_config *session_config,
	struct agm_media_config *media_config,
	struct agm_buffer_config *buffer_config);

/**
  * \brief Close the session.
  *
  * \param[in] handle - Valid session handle obtained
  *       from agm_session_open
  *
  * \return 0 on success, error code otherwise
  */
int agm_session_close(struct session_obj *handle);

/**
  * \brief prepare the session.
  *
  * \param[in] handle - Valid session handle obtained
  *       from agm_session_open
  *
  * \return 0 on success, error code otherwise
  */
int agm_session_prepare(struct session_obj *handle);

/**
  * \brief Start the session.
  *
  * \param[in] handle - Valid session handle obtained
  *       from agm_session_open
  *
  * \return 0 on success, error code otherwise
  */

int agm_session_start(struct session_obj *handle);

/**
  * \brief Stop the session. session must be in started/paused
  *        state before stopping.
  *
  * \param[in] handle - Valid session handle obtained
  *       from agm_session_open
  *
  * \return 0 on success, error code otherwise
  */
int agm_session_stop(struct session_obj *handle);

/**
  * \brief Pause the session. session must be in started state
  *        before resuming.
  *
  * \param[in] handle - Valid session handle obtained
  *       from agm_session_open
  *
  * \return 0 on success, error code otherwise
  */
int agm_session_pause(struct session_obj *handle);

/**
  * \brief Resume the session. session must be in paused state
  *        before resuming.
  *
  * \param[in] handle - Valid session handle obtained
  *       from agm_session_open
  *
  * \return 0 on success, error code otherwise
  */
int agm_session_resume(struct session_obj *handle);

/**
  * \brief Read data buffers.from session
  *
  * \param[in] handle: session handle returned from
  *  	 agm_session_open
  * \param[in,out] buff: buffer where data will be copied to
  * \param[out] count: actual number of bytes filled into
  * the buffer.
  *
  * \return 0 on success, error code otherwise
  */
int agm_session_read(struct session_obj *handle, void *buff, size_t count);

/**
 * \brief Write data buffers.to session
 *
 * \param[in] handle: session handle returned from
 *  	 agm_session_open
 * \param[in] buff: buffer where data will be copied from
 * \param[in] count: actual number of bytes in the buffer.
 *
 * \return 0 on success, error code otherwise
 */
int agm_session_write(struct session_obj *handle, void *buff, size_t count);

/**
  * \brief Get count of Buffer processed by h/w
  *
  * \param[in] handle: session handle returned from
  *  	 agm_session_open
  * \param[in] dir: indicates whether to return the write or
  *       read buffer count
  *
  * \return:  An increasing count of buffers, value wraps back to zero
  * once it reaches SIZE_MAX
  */
size_t agm_get_hw_processed_buff_cnt(struct session_obj *handle, enum direction dir);

/**
  * \brief Get list of AIF info objects.
  *
  * \param [in] aif_list: list of aif_info objects
  * \param [in,out] num_aif_info: number of aif info items in the list.
  * 			if num_aif_info value is listed as zero, AGM will update num_aif_info with
  * 			the number of aif info items in AGM.
  * 			if num_aif_info is greater than zero,
  * 			AGM will copy client specified num_aif_info of items into aif_list.
  *
  * \return: 0 on success, error code otherwise
  */
int agm_get_aif_info_list(struct aif_info *aif_list, size_t *num_aif_info);

/**
  * \brief Set loopback between capture and playback sessions
  *
  * \param[in] capture_session_id : a non zero capture session id
  * \param[in] playback_session_id: playback session id.
  * 				A non zero value enables the loopback.
  * 				A zero value disables the loopback.
  * \param[in] state : flag to indicate to enable(true) or disable(false) loopback
  *
  * \return: 0 on success, error code otherwise
  */
int agm_session_set_loopback(uint32_t capture_session_id, uint32_t playback_session_id, bool state);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* _AGM_INTF_H_ */
