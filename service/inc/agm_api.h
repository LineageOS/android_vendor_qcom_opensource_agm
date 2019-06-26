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
 * Metadata Key Vectors
 */
struct agm_meta_data {
    /**
      * number of GKVs
      */
	uint32_t num_gkvs;

    /**
      * number of CKVs
      */
	uint32_t num_ckvs;

	/**
	  * key values containing GKV followed by CKVs.
	  */
	struct agm_key_value kv[];
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
    size_t size;    /**< size of each buffer */
};

/**
 * Maps the modules instance id to module id for a single module
 */
struct agm_module_id_iid_map {
	uint32_t module_id;  /**< module id */
	uint32_t module_iid; /**< globally unique module instance id */
};

/**
 * Structure which holds tag and corresponding modules tagged with tag id
 */
struct agm_tag_info {
	uint32_t tag_id;                      /**< tag id */
	uint32_t num_modules;                 /**< number of modules matching the tag_id */
	struct agm_module_id_iid_map mid_iid_list[0]; /**< agm_module_id_iid_map list */
};

/**
 * Structure which holds tag info of a given graph.
 */
struct agm_tag_module_info_list {
	uint32_t num_tags;          /**< number of tags */
	uint8_t tag_info_list[];	/**< variable payload of type struct agm_tag_module_info */
};

/**
 * Structure which holds tag config for setparams
 */
struct agm_tag_config {
	uint32_t tag;                /**< tag id */
	uint32_t num_tkvs;           /**< num  of tag key values*/
	struct agm_key_value kv[];   /**< tag key vector*/
};

struct agm_cal_config {
	uint32_t num_ckvs;         /**< num  of tag key values*/
	struct agm_key_value kv[]; /**< tag key vector*/
};

/**
 * Event types
 */
enum event_type {
	AGM_EVENT_DATA_PATH = 1,         /**< Events on the Data path, READ_DONE or WRITE_DONE */
	AGM_EVENT_MODULE,                /**< Events raised by modules */
};

/**
 * Event registration structure.
 */
struct agm_event_reg_cfg {
	/** Valid instance ID of module */
	uint32_t module_instance_id;

	/** Valid event ID of the module */
	uint32_t event_id;

	/**
	 * Size of the event config data based upon the	module_instance_id/event_id
	 * combination.	@values > 0 bytes, in multiples of	4 bytes atleast
	 */
	uint32_t event_config_payload_size;

	/**
	 * 1 - to register the event
	 * 0 - to de-register the event
	 */
	uint8_t is_register;

	/**
	 * module specifc event registration payload
	 */
	uint8_t event_config_payload[];
};

/** Data Events that will be notified to client from AGM */
enum agm_event_id {
	/**
	 * Indicates buffer provided as part of read call has been filled.
	 */
	AGM_EVENT_READ_DONE = 0x1,
	/**
	 * Indicates buffer provided as part of write has been consumed
	 */
	AGM_EVENT_WRITE_DONE = 0x2,

	AGM_EVENT_ID_MAX
};

/** data that will be passed to client in the event callback */
struct agm_event_cb_params {
	 /**< identifies the module which generated event */
	uint32_t source_module_id;

	 /**< identifies the event */
	uint32_t event_id;

	 /**< size of payload below */
	uint32_t event_payload_size;

	 /**< payload associated with the event if any */
	uint8_t event_payload[];
};

/**
 * \brief Callback function signature for events to client
 *
 * \param[in] session_id - Valid audio session id
 * \param[in] event_params - holds all event related info
 * \param[in] client_data - client data set during callback registration.
 */
typedef void (*agm_event_cb)(uint32_t session_id, struct agm_event_cb_params *event_params, void *client_data);

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
  * \param[in] aif_id - Valid audio interface id
  * \param[in] media_config - valid media configuration for the
  *       audio interafce.
  *
  *  \return 0 on success, error code on failure.
  *       If the audio interface is already in use and the
  *       new media_config is different from previous, api will return
  *       failure.
  */
int agm_aif_set_media_config(uint32_t aif_id,
                                    struct agm_media_config *media_config);


 /**
  * \brief Set metadata for an audio interface.
  *
  * \param[in] aif_id - Valid audio interface id
  * \param[in] metadata - valid metadata for the audio
  *       interafce.
  *
  *  \return 0 on success, error code on failure.
  *       If the audio interface is already in use and the
  *       new meta data is set, api will return
  *       failure.
  */
int agm_aif_set_metadata(uint32_t aif_id,
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
  * \param[in] session_id - Valid session id
  * \param[in] aif_id - Valid audio interface id
  * \param[in] metadata - valid metadata for the session and
  *            audio_intf.
  *
  *  \return 0 on success, error code on failure.
  *       If the session is already opened and the new
  *       meta data is set, api will return failure.
  */
int agm_session_aif_set_metadata(uint32_t session_id,
                                           uint32_t aif_id,
                                           struct agm_meta_data *metadata);

/**
 * \brief Set metadata for the session, audio intf pair.
 *
 * \param[in] session_id - Valid audio session id
 * \param[in] aif_id - Valid audio interface id
 * \param[in] state - Connect or Disconnect AIF to Session
 *
 *  \return 0 on success, error code on failure.
 *       If the session is already opened and the new
 *       meta data is set, api will return failure.
 */
int agm_session_aif_connect(uint32_t session_id,
	uint32_t aif_id,
	bool state);

/**
 * \brief Set metadata for the session, audio intf pair.
 *
 * \param[in] session_id - Valid audio session id
 * \param[in] aif_id - Valid audio interface id
 * \param[in] payload - payload containing tag module info list in the graph.
 *           The memory for this pointer is allocated by client.
 * \param [in,out] size: size of the payload.
 * 			if the value of size is zero, AGM will update required module
 * 			info list of a given graph.
 * 			if size equal or greater than the required size,
 * 			AGM will copy the module info.
 *
 *  \return 0 on success, error code on failure.
 */
int agm_session_aif_get_tag_module_info(uint32_t session_id,
		uint32_t aif_id, void *payload, size_t *size);

/**
 * \brief Set parameters for modules in b/w stream and audio interface
 *
 * \param[in] session_id - Valid audio session id
 * \param[in] aif_id - Valid audio interface id
 * \param[in] payload - payload
 * \param[in] size - payload size in bytes
 *
 *  \return 0 on success, error code on failure.
 *       If the session is already opened and the new
 *       meta data is set, api will return failure.
 */
int agm_session_aif_set_params(uint32_t session_id,
	uint32_t aif_id,
	void* payload, size_t size);

/**
 * \brief Set calibration for modules in b/w stream and audio interface
 *
 * \param[in] session_id - Valid audio session id
 * \param[in] aif_id - Valid audio interface id
 * \param[in] cal_config - calibration key vector
 *
 *  \return 0 on success, error code on failure.
 */
int agm_session_aif_set_cal(uint32_t session_id,
	uint32_t aif_id,
	struct agm_cal_config *cal_config);

/**
 * \brief Set parameters for modules in stream
 *
 * \param[in] session_id - Valid audio session id
 * \param[in] payload - payload
 * \param[in] size - payload size in bytes
 *
 *  \return 0 on success, error code on failure.
 *       If the session is already opened and the new
 *       meta data is set, api will return failure.
 */
int agm_session_set_params(uint32_t session_id,
	void* payload, size_t size);

/**
 * \brief Set parameters for modules in b/w stream and audio interface
 *
 * \param[in] session_id - Valid audio session id
 * \param[in] aif_id - valid audio interface id
 * \param[in] tag_config - tag config structure with tag id and tag key vector
 *
 *  \return 0 on success, error code on failure.
 *       If the session is already opened and the new
 *       meta data is set, api will return failure.
 */

int agm_set_params_with_tag(uint32_t session_id, uint32_t aif_id, struct agm_tag_config *tag_config);

/**
  * \brief Open the session with specified session id.
  *
  * \param[in] session_id - Valid audio session id
  * \param[in] cb - callback function to be invoked when an event occurs.
  * \param[in] evt_type - event type that
  * \param[in] client_data - client data.
  *
  * \return 0 on success, error code otherwise
  */
int agm_session_register_cb(uint32_t session_id, agm_event_cb cb, enum event_type evt_type, void *client_data);

/**
  * \brief Register for events from Modules. Not needed for data path events.
  *
  * \param[in] session_id - Valid audio session id
  * \param[out] evt_reg_info - event specific configuration.
  *
  * \return 0 on success, error code otherwise
  */
int agm_session_register_for_events(uint32_t session_id, struct agm_event_reg_cfg *evt_reg_cfg);

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
  * \param[in] state : flag to indicate to enable(true) or disable(false) loopback
  *
  * \return: 0 on success, error code otherwise
  */
int agm_session_set_loopback(uint32_t capture_session_id, uint32_t playback_session_id, bool state);

/**
  * \brief Set ec reference on capture session
  *
  * \param[in] capture_session_id : a non zero capture session id
  * \param[in] aif_id: aif_id on RX path.
  * \param[in] state : flag to indicate to enable(true) or disable(false) ec_ref
  *
  * \return: 0 on success, error code otherwise
  */
int agm_session_set_ec_ref(uint32_t capture_session_id, uint32_t aif_id, bool state);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* _AGM_INTF_H_ */
