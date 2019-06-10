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
#define LOGTAG "AGM: session"

#include <malloc.h>
#include "session_obj.h"
#include "utils.h"

//forward declarations
static int session_close(struct session_obj *sess_obj);
static int session_set_loopback(struct session_obj *sess_obj, uint32_t session_id, bool enable);

static struct aif *aif_obj_get_from_pool(struct session_obj *sess_obj, uint32_t aif)
{
	struct listnode *node;
	struct aif *aif_node;

	list_for_each(node, &sess_obj->aif_pool) {
		aif_node = node_to_item(node, struct aif, node);
		if (aif_node->aif_id == aif)
			return aif_node;
	}

	return NULL;
}

static struct aif* aif_obj_create(struct session_obj *sess_obj, int aif_id)
{
	struct aif *aif_obj = NULL;
	struct device_obj *dev_obj = NULL;
	int ret = 0;

	aif_obj = calloc(1, sizeof(struct aif));
	if (!aif_obj) {
		AGM_LOGE("%s: Memory allocation failed for aif object\n", __func__);
		return aif_obj;
	}

	ret = device_get_obj(aif_id, &dev_obj);
	if (ret || !dev_obj) {
		AGM_LOGE("%s: Error:%d retrieving device object with id:%d \n", __func__, ret, aif_obj->aif_id);
		goto done;
	}
	aif_obj->aif_id = aif_id;
	aif_obj->dev_obj = dev_obj;

done:
	return aif_obj;
}

/* returns aif_obj associated with aif id in the session obj */
int aif_obj_get(struct session_obj *sess_obj, int aif_id, struct aif **aif_obj)
{
	// return from list, if not there create, add to list and return
	struct aif *tobj = NULL;
	int ret = 0;

	tobj = aif_obj_get_from_pool(sess_obj, aif_id);
	if (!tobj) {
		//AGM_LOGE("%s: Couldnt find a aif object in the list, creating one\n", __func__);

		tobj = aif_obj_create(sess_obj, aif_id);
		if (!tobj) {
			AGM_LOGE("%s: Couldnt create an aif object\n", __func__);
			ret = -ENOMEM;
			return ret;
		}
		list_add_tail(&sess_obj->aif_pool, &tobj->node);
	}

	*aif_obj = tobj;

	return ret;
}

/* returns aif_obj associated with aif id in the session obj with state not as specified in the argument */
uint32_t aif_obj_get_count_with_state(struct session_obj *sess_obj, enum aif_state state, bool exact_state_match)
{
	uint32_t count = 0;
	struct listnode *node;
	struct aif *temp = NULL;

	//check how many devices in connected state
	list_for_each(node, &sess_obj->aif_pool) {
		temp = node_to_item(node, struct aif, node);
		if (!temp) {
			AGM_LOGE("%s Error could not find aif node\n", __func__);
			continue;
		}

		if ((exact_state_match  && temp->state == state) ||
			(!exact_state_match && temp->state >= state)) {
			count++;
		}
	}

	return count;
}

static struct agm_meta_data* session_get_merged_metadata(struct session_obj *sess_obj)
{
	struct agm_meta_data *merged = NULL;
	struct agm_meta_data *temp = NULL;
	struct listnode *node;
	struct aif *aif_node;

	list_for_each(node, &sess_obj->aif_pool) {
		aif_node = node_to_item(node, struct aif, node);
		merged = metadata_merge(4, temp, &sess_obj->sess_meta, &aif_node->sess_aif_meta, &aif_node->dev_obj->metadata);
		metadata_free(temp);
		temp = merged;
	}

	return merged;
}

static int session_pool_init()
{
	int ret = 0;
	sess_pool = calloc(1, sizeof(struct session_pool));
	if (!sess_pool) {
		AGM_LOGE("%s: No Memory to create sess_pool\n", __func__);
		ret = -ENOMEM;
		goto done;
	}
	list_init(&sess_pool->session_list);
	pthread_mutex_init(&sess_pool->lock, (const pthread_mutexattr_t *) NULL);

done:
	return ret;
}

static void aif_free(struct aif *aif_obj)
{
	metadata_free(&aif_obj->sess_aif_meta);
	free(aif_obj->params);
	free(aif_obj);
}

static void aif_pool_free(struct session_obj *sess_obj)
{
	struct aif *aif_obj;
	struct listnode *node, *next;

	list_for_each_safe(node, next, &sess_obj->aif_pool) {
		aif_obj = node_to_item(node, struct aif, node);
		if (aif_obj) {
			list_remove(&aif_obj->node);
			aif_free(aif_obj);

		}
	}
}

static void sess_obj_free(struct session_obj *sess_obj)
{
	aif_pool_free(sess_obj);
	metadata_free(&sess_obj->sess_meta);
	free(sess_obj->params);
	free(sess_obj);
}

static void session_pool_free()
{
	struct session_obj *sess_obj;
	struct listnode *node, *next;
	int ret = 0;

	pthread_mutex_lock(&sess_pool->lock);
	list_for_each_safe(node, next, &sess_pool->session_list) {
		sess_obj = node_to_item(node, struct session_obj, node);
		if (sess_obj) {
			pthread_mutex_lock(&sess_obj->lock);
			ret = session_close(sess_obj);
			if (ret) {
				AGM_LOGE("%s, Error:%d closing session with session id:%d\n", __func__, ret, sess_obj->sess_id);
			}
			pthread_mutex_unlock(&sess_obj->lock);

			//cleanup aif pool from session_object
			list_remove(&sess_obj->node);
			sess_obj_free(sess_obj);
		}
	}
	pthread_mutex_unlock(&sess_pool->lock);
}

static struct session_obj* session_obj_create(int session_id)
{
	struct session_obj *obj = NULL;

	obj = calloc(1, sizeof(struct session_obj));
	if (!obj) {
		AGM_LOGE("%s: Memory allocation failed for sesssion object\n", __func__);
		return obj;
	}

	obj->sess_id = session_id;
	list_init(&obj->aif_pool);

	if ( pthread_mutex_init(&obj->lock, (const pthread_mutexattr_t *) NULL))
		AGM_LOGE("mutex init failed\n");

	return obj;
}

struct session_obj *session_obj_get_from_pool(uint32_t session_id)
{
	struct session_obj *obj = NULL;
	struct listnode *node;

	pthread_mutex_lock(&sess_pool->lock);
	list_for_each(node, &sess_pool->session_list) {
		obj = node_to_item(node, struct session_obj, node);
		if (obj->sess_id == session_id)
			break;
		else
			obj = NULL;
	}

	if (!obj) {
		//AGM_LOGE("%s: Couldnt find a session object in the list, creating one\n",  __func__);
		obj = session_obj_create(session_id);
		if (!obj) {
			AGM_LOGE("%s: Couldnt create a session object\n",  __func__);
			goto done;
		}
		list_add_tail(&sess_pool->session_list, &obj->node);
	}

done:
	pthread_mutex_unlock(&sess_pool->lock);
	return obj;
}

/* returns session_obj associated with session id */
int session_obj_get(int session_id, struct session_obj **obj)
{
	// return from list, if not there create, add to list and return
	struct session_obj *tobj = NULL;
	int ret = 0;

	tobj = session_obj_get_from_pool(session_id);
	if (!tobj) {
		AGM_LOGE("%s, Couldnt find or create session_obj\n", __func__);
		ret = -ENOMEM;
	}

	*obj = tobj;
	return ret;
}

static int session_set_loopback(struct session_obj *sess_obj, uint32_t session_id, bool enable)
{
	int ret = 0;
	struct session_obj *pb_obj = NULL;
	uint32_t pb_id = 0;
	struct aif *pb_aif_obj = NULL, *temp = NULL;
	struct listnode *node;
	struct device_obj *pb_dev_obj = NULL;
	struct agm_meta_data *capture_metadata = NULL;
	struct agm_meta_data *playback_metadata = NULL;
	struct agm_meta_data *merged_metadata = NULL;

	//TODO:
	/*
	 * 1. merged metadata of pb session + cap session
	 * 2. call graph_add
	 * 3. call start (prepare doesnt achieve anything so skip)
	 * 4. Expectation for loopback is that its establishing an edge b/w TX and RX session.
	 *    So no new modules/subgraphs which require configuration is expected and hence
	 *    no separate setparams() for loopback for now.
	 */
	pb_id = (session_id != 0) ? session_id : sess_obj->loopback_sess_id;
	ret = session_obj_get(pb_id, &pb_obj);
	if (ret) {
		AGM_LOGE("%s: Error:%d getting session object with session id:%d\n", __func__, ret, pb_id);
		goto done;
	}

        /*For now this logic only supports on device connected on the playback path, need to revisit this
          for one TX connetect to multiple RX paths loopback*/
	list_for_each(node, &pb_obj->aif_pool) {
		temp = node_to_item(node, struct aif, node);
		if (temp->state == AIF_OPEN) {
			pb_aif_obj = temp;
			break;
		}
	}

        if (pb_aif_obj)
            pb_dev_obj = pb_aif_obj->dev_obj;

	capture_metadata = session_get_merged_metadata(sess_obj);
	if (!capture_metadata) {
		ret = -ENOMEM;
		AGM_LOGE("%s: Error:%d, merging metadata with session id=%d\n", __func__, ret, sess_obj->sess_id);
		goto done;
	}

	playback_metadata = session_get_merged_metadata(pb_obj);
	if (!playback_metadata) {
		ret = -ENOMEM;
		AGM_LOGE("%s: Error:%d, merging metadata with session id=%d\n", __func__, ret, pb_id);
		goto done;
	}

	merged_metadata = metadata_merge(2, capture_metadata, playback_metadata);
	if (!merged_metadata) {
		ret = -ENOMEM;
		AGM_LOGE("%s: Error:%d, merging metadata with playback session id=%d and capture session id=%d\n",
				__func__, ret, pb_id, sess_obj->sess_id);
		goto done;
	}

	if (enable)
		ret = graph_add(sess_obj->graph, merged_metadata, pb_dev_obj);
	else
		ret = graph_remove(sess_obj->graph, merged_metadata);

	if (ret) {
		AGM_LOGE("%s: Error:%d graph %s failed for session_id: %d\n",
			__func__, ret, ((pb_id != 0) ? "add":"remove"), sess_obj->sess_id);
		goto done;
	}

done:
	metadata_free(capture_metadata);
	metadata_free(playback_metadata);
	metadata_free(merged_metadata);
	return ret;
	}

static int session_disconnect_aif(struct session_obj *sess_obj, struct aif *aif_obj, uint32_t opened_count)
{
	int ret = 0;
	struct agm_meta_data *merged_metadata = NULL;
	struct graph_obj *graph = sess_obj->graph;

	// merge metadata
	merged_metadata = metadata_merge(3, &sess_obj->sess_meta, &aif_obj->sess_aif_meta, &aif_obj->dev_obj->metadata);

	if (opened_count == 1) {
		//TODO: use new stop prime here
		ret = graph_stop(graph);
		if (ret) {
			AGM_LOGE("%s: Error:%d graph close failed session_id: %d, audio interface id:%d \n",
				__func__, ret, sess_obj->sess_id, aif_obj->aif_id);
		}
	} else {
		ret = graph_remove(graph, merged_metadata);
		if (ret) {
			AGM_LOGE("%s: Error:%d graph remove failed session_id: %d, audio interface id:%d \n",
				__func__, ret, sess_obj->sess_id, aif_obj->aif_id);
		}
	}

	ret = device_close(aif_obj->dev_obj);
	if (ret) {
		AGM_LOGE("%s: Error:%d closing device object with id:%d \n",
			__func__, ret, aif_obj->aif_id);
	}

	free(merged_metadata->ckv.kv);
	free(merged_metadata->gkv.kv);
	free(merged_metadata);
	return ret;
}

static int session_connect_aif(struct session_obj *sess_obj, struct aif *aif_obj, uint32_t opened_count)
{
	int ret = 0;
	struct agm_meta_data *merged_metadata = NULL;
	struct graph_obj *graph = sess_obj->graph;

        AGM_LOGE("entry sess id %d aif id %d", sess_obj->sess_id, aif_obj->aif_id);
	//step 2.a  merge metadata
	merged_metadata = metadata_merge(3, &sess_obj->sess_meta, &aif_obj->sess_aif_meta, &aif_obj->dev_obj->metadata);

	ret = device_open(aif_obj->dev_obj);
	if (ret) {
		AGM_LOGE("%s: Error:%d opening device object with id:%d \n",
			__func__, ret, aif_obj->aif_id);
		goto done;
	}

	//step 2.b
	if (opened_count == 0) {
		if (sess_obj->state == SESSION_CLOSED) {
			ret = graph_open(merged_metadata, sess_obj, aif_obj->dev_obj, &sess_obj->graph);
			graph = sess_obj->graph;
			if (ret) {
				AGM_LOGE("%s: Error:%d graph open failed session_id: %d, audio interface id:%d \n",
					__func__, ret, sess_obj->sess_id, aif_obj->aif_id);
				goto close_device;
			}
		} else {
			ret = graph_change(graph, merged_metadata, aif_obj->dev_obj);
			if (ret) {
				AGM_LOGE("%s: Error:%d graph change failed session_id: %d, audio interface id:%d \n",
					__func__, ret, sess_obj->sess_id, aif_obj->aif_id);
				goto close_device;
			}
		}
	} else {
			ret = graph_add(graph, merged_metadata, aif_obj->dev_obj);
			if (ret) {
				AGM_LOGE("%s: Error:%d graph add failed session_id: %d, audio interface id:%d \n",
					__func__, ret, sess_obj->sess_id, aif_obj->aif_id);
				goto close_device;
			}
	}

	//step 2.c set cached params for stream only in closed
	if (sess_obj->state == SESSION_CLOSED && sess_obj->params != NULL) {
		ret = graph_set_config(graph, sess_obj->params, sess_obj->params_size);
		if (ret) {
			AGM_LOGE("%s: Error:%d setting session cached params: %d\n",
				__func__, ret, sess_obj->sess_id);
			goto graph_cleanup;
		}
	}

	//step 2.c set cached device(streamdevice + device) params
	if (aif_obj->params != NULL) {
		ret = graph_set_config(graph, aif_obj->params, aif_obj->params_size);
		if (ret) {
			AGM_LOGE("%s: Error:%d setting session cached params: %d\n",
				__func__, ret, sess_obj->sess_id);
			goto graph_cleanup;
		}
	}

	goto done;

graph_cleanup:
	if (opened_count == 0)
		graph_close(sess_obj->graph);
	else 
		graph_remove(sess_obj->graph, merged_metadata);

close_device:
	device_close(aif_obj->dev_obj);

done:
        AGM_LOGE("exit");
	free(merged_metadata->ckv.kv);
	free(merged_metadata->gkv.kv);
	free(merged_metadata);
	return ret;
}

static int session_open_with_first_device(struct session_obj *sess_obj)
{
	int ret = 0;
	struct aif *aif_obj = NULL, *temp = NULL;
	struct listnode *node;

	list_for_each(node, &sess_obj->aif_pool) {
		temp = node_to_item(node, struct aif, node);
		if (temp->state == AIF_OPEN) {
			aif_obj = temp;
			break;
		}
	}

	if (!aif_obj) {
		AGM_LOGE("%s: No Audio interface(Backend) set on session(Frontend):%d\n",
				__func__, sess_obj->sess_id);
		ret = -EPIPE;
		goto done;
	}

	ret = session_connect_aif(sess_obj, aif_obj, 0);
	if (ret) {
		AGM_LOGE("%s: Audio interface(Backend):%d <-> session(Frontend):%d Connect failed error:%d\n",
				__func__,  aif_obj->aif_id, sess_obj->sess_id, ret);
		goto done;
	}
	aif_obj->state = AIF_OPENED;

done:
	return ret;
}

static int session_connect_reminder_devices(struct session_obj *sess_obj)
{
	int ret = 0;
	struct aif *aif_obj = NULL;
	struct listnode *node;
	uint32_t opened_count = 0;

	// opened_count is 1 because this function is being called after connecting with 1 device
	opened_count = 1;

	list_for_each(node, &sess_obj->aif_pool) {
		aif_obj = node_to_item(node, struct aif, node);
		if (aif_obj && aif_obj->state == AIF_OPEN) {
			ret = session_connect_aif(sess_obj, aif_obj, opened_count);
			if (ret) {
				AGM_LOGE("%s: Audio interface(Backend): %d <-> session(Frontend): %d Connect failed error:%d\n",
						__func__, aif_obj->aif_id, sess_obj->sess_id, ret);
				goto unwind;
			}

			aif_obj->state = AIF_OPENED;
			opened_count++;
		}
	}

	return 0;

unwind:
	list_for_each(node, &sess_obj->aif_pool) {
		aif_obj = node_to_item(node, struct aif, node);
		if (aif_obj && aif_obj->state == AIF_OPENED) {
			/*TODO: fix the 3rd argument to provide correct count*/
			ret = session_disconnect_aif(sess_obj, aif_obj, 1);
			if (ret) {
				AGM_LOGE("%s: Error:%d initializing session_pool\n", __func__, ret);
			}
			aif_obj->state = AIF_OPEN;
			opened_count--;
		}
	}
	ret = graph_close(sess_obj->graph);
	if (ret) {
		AGM_LOGE("%s: Error:%d initializing session_pool\n", __func__, ret);
	}
	sess_obj->graph = NULL;

	return ret;
}

static int session_prepare(struct session_obj *sess_obj)
{
	int ret = 0;
	struct aif *aif_obj = NULL;
	enum direction dir = sess_obj->stream_config.dir;
	struct listnode *node = NULL;
	uint32_t count = 0;

	count = aif_obj_get_count_with_state(sess_obj, AIF_OPENED, false);
	if (count == 0) {
		AGM_LOGE("%s Error:%d No aif in right state to proceed with session start for sessionid :%d\n", __func__, ret, sess_obj->sess_id);
		return -1; //-EINVALID;
	}

	if (dir == TX) {
		ret = graph_prepare(sess_obj->graph);
		if (ret) {
			AGM_LOGE("%s Error:%d preparing graph\n", __func__, ret);
			goto done;
		}
	}
	
	list_for_each(node, &sess_obj->aif_pool) {
		aif_obj = node_to_item(node, struct aif, node);
		if (!aif_obj) {
			AGM_LOGE("%s Error:%d could not find aif node\n", __func__, ret);
			goto done;
		}

		//TODO 1: in device switch cases, only the aif not prepared should be prepared.
		if (aif_obj->state == AIF_OPENED || aif_obj->state == AIF_STOPPED) {
			ret = device_prepare(aif_obj->dev_obj);
			if (ret) {
				AGM_LOGE("%s Error:%d preparing device\n", __func__, ret);
				goto done;
			}
			aif_obj->state = AIF_PREPARED;
		}
	} 

	if (dir == RX) {
		ret = graph_prepare(sess_obj->graph);
		if (ret) {
			AGM_LOGE("%s Error:%d preparing graph\n", __func__, ret);
			goto done;
		}
	}

	sess_obj->state = SESSION_PREPARED;
	return ret;

done:
	return ret;
}

static int session_start(struct session_obj *sess_obj)
{
	int ret = 0;
	struct aif *aif_obj = NULL;
	enum direction dir = sess_obj->stream_config.dir;
	struct listnode *node = NULL;
	uint32_t count = 0;

	count = aif_obj_get_count_with_state(sess_obj, AIF_OPENED, false);
	if (count == 0) {
		AGM_LOGE("%s Error:%d No aif in right state to proceed with session start for session id :%d\n", __func__, ret, sess_obj->sess_id);
		return -1; //-EINVALID;
	}

	if (dir == TX) {
		ret = graph_start(sess_obj->graph);
		if (ret) {
			AGM_LOGE("%s Error:%d starting graph\n", __func__, ret);
			goto done;
		}
	}

	list_for_each(node, &sess_obj->aif_pool) {
		aif_obj = node_to_item(node, struct aif, node);
		if (!aif_obj) {
			AGM_LOGE("%s Error:%d could not find aif node\n", __func__, ret);
			goto unwind;
		}

		if (aif_obj->state == AIF_OPENED || aif_obj->state == AIF_PREPARED || aif_obj->state == AIF_STOPPED ) {
			ret = device_start(aif_obj->dev_obj);
			if (ret) {
				AGM_LOGE("%s Error:%d starting device id:%d\n", __func__, ret, aif_obj->aif_id);
				goto unwind;
			}
			aif_obj->state = AIF_STARTED;
		}
	}

	if (dir == RX) {
		ret = graph_start(sess_obj->graph);
		if (ret) {
			AGM_LOGE("%s Error:%d starting graph\n", __func__, ret);
			goto unwind;
		}
	}

	sess_obj->state = SESSION_STARTED;
	return ret;

unwind:

	if (dir == TX)
		graph_stop(sess_obj->graph);

	list_for_each(node, &sess_obj->aif_pool) {
		aif_obj = node_to_item(node, struct aif, node);
		if (aif_obj && (aif_obj->state == AIF_STARTED)) {
			device_stop(aif_obj->dev_obj);
			//If start fails, client will retry with a prepare call, so moving to opened state will allow prepare to go through
			aif_obj->state = AIF_OPENED;
		}
	}

done:
	return ret;
}

static int session_stop(struct session_obj *sess_obj)
{
	int ret = 0;
	struct aif *aif_obj = NULL;
	enum direction dir = sess_obj->stream_config.dir;
	struct listnode *node = NULL;

	if (sess_obj->state != SESSION_STARTED) {
		AGM_LOGE("%s session not in STARTED state, current state:%d\n", __func__, sess_obj->state);
		return -EINVAL;
	}

	if (dir == RX) {
		ret = graph_stop(sess_obj->graph);
		if (ret) {
			AGM_LOGE("%s Error:%d starting graph\n", __func__, ret);
			goto done;
		}
	}

	list_for_each(node, &sess_obj->aif_pool) {
		aif_obj = node_to_item(node, struct aif, node);
		if (!aif_obj) {
			AGM_LOGE("%s Error:%d could not find aif node\n", __func__, ret);
			continue;
		}

		if (aif_obj->state == AIF_STARTED) {
			ret = device_stop(aif_obj->dev_obj);
			if (ret) {
				AGM_LOGE("%s Error:%d stopping device id:%d\n", __func__, ret, aif_obj->aif_id);

			}
			aif_obj->state = AIF_STOPPED;
		}
	}

	if (dir == TX) {
		ret = graph_stop(sess_obj->graph);
		if (ret) {
			AGM_LOGE("%s Error:%d stopping graph\n", __func__, ret);
		}
	}

	sess_obj->state = SESSION_STOPPED;
	return ret;

done:
	return ret;
}

static int session_close(struct session_obj *sess_obj)
{
	int ret = 0;
	struct aif *aif_obj = NULL;
	struct listnode *node = NULL;

	if (sess_obj->state == SESSION_CLOSED) {
		AGM_LOGE("%s session already in CLOSED state\n", __func__);
		return -EALREADY;
	}
        AGM_LOGE("entry sess_id %d", sess_obj->sess_id);
	ret = graph_close(sess_obj->graph);
	if (ret) {
		AGM_LOGE("%s Error:%d closing graph\n", __func__, ret);
	}
	sess_obj->graph = NULL;

	list_for_each(node, &sess_obj->aif_pool) {
		aif_obj = node_to_item(node, struct aif, node);
		if (!aif_obj) {
			AGM_LOGE("%s Error:%d could not find aif node\n", __func__, ret);
			continue;
		}

		if (aif_obj->state >= AIF_OPENED) {
			ret = device_close(aif_obj->dev_obj);
			if (ret) {
				AGM_LOGE("%s Error:%d stopping device id:%d\n", __func__, ret, aif_obj->aif_id);
			}
			aif_obj->state = AIF_CLOSED;
		}
	}

	sess_obj->state = SESSION_CLOSED;
        AGM_LOGE("exit");
	return ret;
}


int session_obj_deinit()
{
	session_pool_free();
	device_deinit();
	graph_deinit();
	return 0;
}

/* Initializes session_obj, enumerate and fill session related information */
int session_obj_init()
{
	int ret = 0;

	ret = device_init();
	if (ret) {
		AGM_LOGE("%s: Error:%d initializing device\n", __func__, ret);
		goto done;
	}

	ret = graph_init();
	if (ret) {
		AGM_LOGE("%s: Error:%d initializing graph\n", __func__, ret);
		goto device_deinit;
	}

	ret = session_pool_init();
	if (ret) {
		AGM_LOGE("%s: Error:%d initializing session_pool\n", __func__, ret);
		goto graph_deinit;
	}
	goto done;

graph_deinit:
	graph_deinit();

device_deinit:
	device_deinit();

done:
	return ret;
}

int session_obj_set_sess_metadata(struct session_obj *sess_obj, struct agm_meta_data *metadata)
{

	int ret = 0;

	pthread_mutex_lock(&sess_obj->lock);
	ret = metadata_copy(&(sess_obj->sess_meta), metadata);
	pthread_mutex_unlock(&sess_obj->lock);

	return ret;
}


int session_obj_set_sess_aif_metadata(struct session_obj *sess_obj,
	uint32_t aif_id, struct agm_meta_data *metadata)
{

	int ret = 0;
	struct aif *aif_obj = NULL;

	pthread_mutex_lock(&sess_obj->lock);
	ret = aif_obj_get(sess_obj, aif_id, &aif_obj);
	if (ret) {
		AGM_LOGE("%s: Error obtaining aif object with sess_id:%d,  aif id:%d\n",
			__func__, sess_obj->sess_id, aif_id);
		goto done;
	}

	ret = metadata_copy(&(aif_obj->sess_aif_meta), metadata);
	if (ret) {
		AGM_LOGE("%s: Error copying session audio interface metadata sess_id:%d, aif_id:%d \n",
			 __func__, sess_obj->sess_id, aif_obj->aif_id);
	}

done:
	pthread_mutex_unlock(&sess_obj->lock);
	return ret;
}

int session_obj_sess_aif_connect(struct session_obj *sess_obj,
	uint32_t aif_id, bool aif_state)
{
	int ret = 0;
	struct aif *aif_obj = NULL;
	uint32_t opened_count = 0;

	pthread_mutex_lock(&sess_obj->lock);
	ret = aif_obj_get(sess_obj, aif_id, &aif_obj);
	if (ret) {
		AGM_LOGE("%s: Error obtaining aif object with sess_id:%d,  aif id:%d\n",
			 __func__, sess_obj->sess_id, aif_id);
		goto done;
	}

	if (((aif_state == true) && (aif_obj->state > AIF_OPENED)) ||
		((aif_state == false) && (aif_obj->state < AIF_OPEN))) {
		AGM_LOGE("%s AIF already in state %d\n", __func__, aif_obj->state);
		ret = -EALREADY;
		goto done;
	}

	opened_count = aif_obj_get_count_with_state(sess_obj, AIF_OPENED, false);

	if (aif_state == true) {
		//TODO: check if the assumption is correct
		//Assumption: Each of the following state assumes that there was an Audio Interface in Connect state.
		switch (sess_obj->state){
		case SESSION_OPENED:
			ret = session_connect_aif(sess_obj, aif_obj, opened_count);
			if (ret) {
				AGM_LOGE("%s Error:%d, Unable to Connect device\n", __func__, ret);
				goto done;
			}
			aif_obj->state = AIF_OPENED;
			opened_count++;
			break;
		case SESSION_PREPARED:
		case SESSION_STOPPED:
			ret = session_connect_aif(sess_obj, aif_obj, opened_count);
			if (ret) {
				AGM_LOGE("%s Error:%d, Unable to Connect device\n", __func__, ret);
				goto done;
			}
			aif_obj->state = AIF_OPENED;
			opened_count++;

			ret = session_prepare(sess_obj);
			if (ret) {
				AGM_LOGE("%s Error:%d, Unable to prepare device\n", __func__, ret);
				goto unwind;
			}

			break;
		case SESSION_STARTED:
			ret = session_connect_aif(sess_obj, aif_obj, opened_count);
			if (ret) {
				AGM_LOGE("%s Error:%d, Unable to Connect device\n", __func__, ret);
				goto done;
			}
			aif_obj->state = AIF_OPENED;
			opened_count++;

			ret = session_prepare(sess_obj);
			if (ret) {
				AGM_LOGE("%s Error:%d, Unable to prepare device\n", __func__, ret);
				goto unwind;
			}
			ret = session_start(sess_obj);
			if (ret) {
				AGM_LOGE("%s Error:%d, Unable to start device\n", __func__, ret);
				goto unwind;
			}
			break;

		case SESSION_CLOSED:
			aif_obj->state = AIF_OPEN;
			break;

		}

	} else {
		aif_obj->state = AIF_CLOSE;

	   //if session is in started state and more than 1 device is connect, call remove, if only 1 device is connected, do graph stop
		switch (sess_obj->state) {
		case SESSION_OPENED:
		case SESSION_PREPARED:
		case SESSION_STARTED:
		case SESSION_STOPPED:
			ret = session_disconnect_aif(sess_obj, aif_obj, opened_count);
			if (ret) {
				AGM_LOGE("%s Error:%d, Unable to Connect device\n", __func__, ret);
				goto done;
			}
		}
		aif_obj->state = AIF_CLOSED;
	}
	goto done;

unwind:
	session_disconnect_aif(sess_obj, aif_obj, opened_count);

done:
	pthread_mutex_unlock(&sess_obj->lock);
	return ret;
}

int session_obj_open(uint32_t session_id, struct session_obj **session)
{

	struct session_obj *sess_obj = NULL;
	int ret = 0;

	ret = session_obj_get(session_id, &sess_obj);
	if (ret) {
		AGM_LOGE("%s: Error getting session object\n", __func__);
		return ret;
	}

	pthread_mutex_lock(&sess_obj->lock);
	if (sess_obj->state != SESSION_CLOSED) {
		AGM_LOGE("%s: Session already Opened, session_state:%d\n", __func__, sess_obj->state);
		ret = -EALREADY;
		goto done;
	}
        AGM_LOGE("entry sess_id %d", session_id);

/*
	1. get first device obj from the list for the session
	2. concatenate stream+dev metadata
	3. for playback, open alsa first, open graph
	4. get rest of the devices, call add graph
	5. update state as opened
*/

	ret = session_open_with_first_device(sess_obj);
	if (ret) {
		AGM_LOGE("%s: Unable to open a session with Session ID:%d\n",  __func__, sess_obj->sess_id);
		goto done;
	}

	ret = session_connect_reminder_devices(sess_obj);
	if (ret) {
		AGM_LOGE("%s: Unable to open a session with Session ID:%d\n",  __func__, sess_obj->sess_id);
		goto done;
	}

	//configure loopback if valid session id has been set
	if (sess_obj->loopback_sess_id != 0) {
		ret = session_set_loopback(sess_obj, sess_obj->loopback_sess_id, true);
		if (ret) {
			goto done;
		}
	}

	sess_obj->state = SESSION_OPENED;
	*session = sess_obj;

done:
	pthread_mutex_unlock(&sess_obj->lock);
	return ret;
}

int session_obj_set_config(struct session_obj *sess_obj, struct agm_session_config *stream_config,
	struct agm_media_config *media_config,
	struct agm_buffer_config *buffer_config)
{

	pthread_mutex_lock(&sess_obj->lock);

	sess_obj->stream_config = *stream_config;
	sess_obj->media_config = *media_config;
	sess_obj->buffer_config = *buffer_config;

	pthread_mutex_unlock(&sess_obj->lock);
	return 0;
}

int session_obj_prepare(struct session_obj *sess_obj)
{
	int ret = 0;

	pthread_mutex_lock(&sess_obj->lock);
	ret = session_prepare(sess_obj);
	pthread_mutex_unlock(&sess_obj->lock);

	return ret;
}

int session_obj_start(struct session_obj *sess_obj)
{
	int ret = 0;

	pthread_mutex_lock(&sess_obj->lock);
	ret = session_start(sess_obj);
	pthread_mutex_unlock(&sess_obj->lock);

	return ret;
}

int session_obj_stop(struct session_obj *sess_obj)
{
	int ret = 0;

	pthread_mutex_lock(&sess_obj->lock);
	ret = session_stop(sess_obj);
	pthread_mutex_unlock(&sess_obj->lock);

	return ret;
}


int session_obj_close(struct session_obj *sess_obj)
{
	int ret = 0;

	pthread_mutex_lock(&sess_obj->lock);
	ret = session_close(sess_obj);
	pthread_mutex_unlock(&sess_obj->lock);

	return ret;
}

int session_obj_pause(struct session_obj *sess_obj)
{
	int ret = 0;

	pthread_mutex_lock(&sess_obj->lock);
	/* TODO: should pause be issued in specific state, for now ensure its in started state */
	if (sess_obj->state != SESSION_STARTED) {
		AGM_LOGE("%s Cannot issue pause in state:%d\n", __func__, sess_obj->state);
		ret = -EINVAL;
		goto done;
	}

	ret = graph_pause(sess_obj->graph);
	if (ret) {
		AGM_LOGE("%s Error:%d pausing graph\n", __func__, ret);
	}

	sess_obj->state = SESSION_PAUSED;

done:
	pthread_mutex_unlock(&sess_obj->lock);
	return ret;
}

int session_obj_resume(struct session_obj *sess_obj)
{
	int ret = 0;

	pthread_mutex_lock(&sess_obj->lock);
	if (sess_obj->state != SESSION_PAUSED) {
		ret = -EINVAL;
		goto done;
	}

	ret = graph_resume(sess_obj->graph);
	if (ret) {
		AGM_LOGE("%s Error:%d resuming graph\n", __func__, ret);
	}

	sess_obj->state = SESSION_STARTED;

done:
	pthread_mutex_unlock(&sess_obj->lock);
	return ret;
}

int session_obj_read(struct session_obj *sess_obj, void *buff, size_t count)
{
	int ret = 0;

	pthread_mutex_lock(&sess_obj->lock);
	if (sess_obj->state == SESSION_CLOSED) {
		AGM_LOGE("%s Cannot issue read in state:%d\n", __func__, sess_obj->state);
		ret = -EINVAL;
		goto done;
	}

	ret = graph_read(sess_obj->graph, buff, count);
	if (ret) {
		AGM_LOGE("%s Error:%d reading from graph\n", __func__, ret);
	}

done:
	pthread_mutex_unlock(&sess_obj->lock);
	return ret;
}

int session_obj_write(struct session_obj *sess_obj, void *buff, size_t count)
{
	int ret = 0;

	pthread_mutex_lock(&sess_obj->lock);
	if (sess_obj->state == SESSION_CLOSED) {
		AGM_LOGE("%s Cannot issue write in state:%d\n", __func__, sess_obj->state);
		ret = -EINVAL;
		goto done;
	}

	ret = graph_write(sess_obj->graph, buff, count);
	if (ret) {
		AGM_LOGE("%s Error:%d writing to graph\n", __func__, ret);
	}

done:
	pthread_mutex_unlock(&sess_obj->lock);
	return ret;
}

size_t session_obj_hw_processed_buff_cnt(struct session_obj *sess_obj, enum direction dir)
{
	int ret = 0;

	pthread_mutex_lock(&sess_obj->lock);
	if (sess_obj->state == SESSION_CLOSED) {
		AGM_LOGE("%s Cannot issue resume in state:%d\n", __func__, sess_obj->state);
		ret = -EINVAL;
		goto done;
	}

	ret = graph_get_hw_processed_buff_cnt(sess_obj->graph, dir);


done:
	pthread_mutex_unlock(&sess_obj->lock);
	return ret;
}

int session_obj_set_loopback(struct session_obj *sess_obj, uint32_t playback_sess_id, bool state)
{
	int ret = 0;

	pthread_mutex_lock(&sess_obj->lock);
	if (playback_sess_id == sess_obj->loopback_sess_id) {
		AGM_LOGE("%s: loopback already %s for session:%d\n",
				__func__, ((playback_sess_id != 0) ? "enabled":"disabled"), sess_obj->sess_id);
		ret = -EINVAL;
		goto done;
	}

	/*
	 * loopback enables just the edge b/w capture and playback sessions.
	 * There is no need to call prepare/start on added graphs as their states are
	 * are updated as each of these session states are updated.
	 */

	switch(sess_obj->state) {
	case SESSION_OPENED:
	case SESSION_PREPARED:
	case SESSION_STARTED:
	case SESSION_STOPPED:
		if (state == true)
			ret = session_set_loopback(sess_obj, playback_sess_id, state);
		else
			ret = session_set_loopback(sess_obj, sess_obj->loopback_sess_id, state);
		break;

	case SESSION_CLOSED:
		break;
	}
	sess_obj->loopback_sess_id = playback_sess_id;

done:
	pthread_mutex_unlock(&sess_obj->lock);
	return ret;
}
