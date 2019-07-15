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
#define LOGTAG "AGM: metadata"
#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "metadata.h"
#include "utils.h"

static void metadata_print(struct agm_meta_data_gsl* meta_data)
{
	int i;
	size_t size  = meta_data->gkv.num_kvs;

	AGM_LOGE("GKV size:%d\n", size);
	for (i = 0; i < size; i++) {
		AGM_LOGE("key:0x%x, value:0x%x ", meta_data->gkv.kv[i].key, meta_data->gkv.kv[i].value);
	}

	size = meta_data->ckv.num_kvs;
	AGM_LOGE("\nCKV size:%d\n", size);
	for (i = 0; i < size; i++) {
		AGM_LOGE("key:0x%x, value:0x%d ", meta_data->ckv.kv[i].key, meta_data->ckv.kv[i].value);
	}
	AGM_LOGE("\n");
}

static void metadata_remove_dup(
		struct agm_meta_data_gsl* meta_data) {

	int i, j, k;
	size_t  size;

	//sort gkvs
	size = meta_data->gkv.num_kvs;
	for (i = 0; i < size; i++) {
		for (j = i + 1; j < size; j++) {
			if (meta_data->gkv.kv[i].key == meta_data->gkv.kv[j].key) {
				for (k = j; k < size; k++) {
					meta_data->gkv.kv[k].key = meta_data->gkv.kv[k + 1].key;
					meta_data->gkv.kv[k].value = meta_data->gkv.kv[k + 1].value;
				}
				size--;
				j--;
			}
		}
	}
	meta_data->gkv.num_kvs = size;

	//sort ckvs
	size = meta_data->ckv.num_kvs;
	for (i = 0; i < size; i++) {
		for (j = i + 1; j < size; j++) {
			if (meta_data->ckv.kv[i].key == meta_data->ckv.kv[j].key) {
				for (k = j; k < size; k++) {
					meta_data->ckv.kv[k].key = meta_data->ckv.kv[k + 1].key;
					meta_data->ckv.kv[k].value = meta_data->ckv.kv[k + 1].value;
				}
				size--;
				j--;
			}
		}
	}
	meta_data->ckv.num_kvs = size;
	//metadata_print(meta_data);
}

void metadata_update_cal(struct agm_meta_data_gsl *meta_data, struct agm_key_vector_gsl *ckv)
{
	int i, j;

	for (i = 0; i < meta_data->ckv.num_kvs; i++) {
		for (j = 0; j < ckv->num_kvs; j++) {
			if (meta_data->ckv.kv[i].key == ckv->kv[j].key) {
				meta_data->ckv.kv[i].value = ckv->kv[j].value;
			}
		}
	}
}

struct agm_meta_data_gsl* metadata_merge(int num, ...)
{
	int total_gkv = 0;
	int total_ckv = 0;
	struct agm_key_value *gkv_offset;
	struct agm_key_value *ckv_offset;
	struct agm_meta_data_gsl *temp, *merged = NULL;

	va_list valist;
	int i = 0;

	va_start(valist, num);
	for (i = 0; i < num; i++) {
		temp = va_arg(valist, struct agm_meta_data_gsl*);
		if (temp) {
			total_gkv += temp->gkv.num_kvs;
			total_ckv += temp->ckv.num_kvs;
		}
	}
	va_end(valist);

	merged = calloc(1, sizeof(struct agm_meta_data_gsl));
	if (!merged) {
		AGM_LOGE("%s: No memory to create merged metadata\n", __func__);
		return NULL;
	}

	merged->gkv.num_kvs = total_gkv;
	merged->ckv.num_kvs = total_ckv;

	merged->gkv.kv = calloc(merged->gkv.num_kvs, sizeof(struct agm_key_value));
	if (!merged->gkv.kv) {
		AGM_LOGE("%s: No memory to create merged metadata gkv\n", __func__);
		free(merged);
		return NULL;
	}

	merged->ckv.kv = calloc(merged->ckv.num_kvs, sizeof(struct agm_key_value));
	if (!merged->ckv.kv) {
		AGM_LOGE("%s No memory to create merged metadata ckv\n", __func__);
		free(merged->gkv.kv);
		free(merged);
		return NULL;
	}

	gkv_offset = merged->gkv.kv;
	ckv_offset = merged->ckv.kv;
	va_start(valist, num);
	for (i = 0; i < num; i++) {
		temp = va_arg(valist, struct agm_meta_data_gsl*);
		if (temp) {
			memcpy(gkv_offset, temp->gkv.kv, temp->gkv.num_kvs * sizeof(struct agm_key_value));
			gkv_offset += temp->gkv.num_kvs;
			memcpy(ckv_offset, temp->ckv.kv, temp->ckv.num_kvs * sizeof(struct agm_key_value));
			ckv_offset += temp->ckv.num_kvs;
		}
	}
	va_end(valist);

	metadata_remove_dup(merged);

	return merged;
}

int metadata_copy(struct agm_meta_data_gsl *dest, struct agm_meta_data *src)
{
	int ret = 0;

	//validate src metadata
	if (!src->num_gkvs || !src->num_ckvs || !src->kv) {
		AGM_LOGE("%s: Invalid input CKV", __func__);
		return -EINVAL;
	}

	// free cached metadata first
	free(dest->ckv.kv);
	dest->ckv.kv = NULL;
	free(dest->gkv.kv);
	dest->gkv.kv = NULL;

	// set new no of kvs
	dest->ckv.num_kvs = src->num_ckvs;
	dest->gkv.num_kvs = src->num_gkvs;

	//allocate new gkv array
	dest->gkv.kv = calloc(src->num_gkvs, sizeof(struct agm_key_value));
	if (!dest->gkv.kv) {
		printf("%s No memory to allocate GKV \n", __func__);
		ret = -ENOMEM;
		goto error;
	}
	memcpy(dest->gkv.kv, src->kv, (src->num_gkvs * sizeof(struct agm_key_value)));

	//allocate new ckv array
	dest->ckv.kv = calloc(src->num_ckvs, sizeof(struct agm_key_value));
	if (!dest->ckv.kv) {
		printf("%s No memory to allocate CKV\n", __func__);
		ret = -ENOMEM;
		goto error;
	}
	memcpy(dest->ckv.kv, (src->kv + src->num_gkvs), (src->num_ckvs * sizeof(struct agm_key_value)));
	//metadata_print(dest);

	return ret;

error:
	free(dest->ckv.kv);
	dest->ckv.kv = NULL;
	free(dest->gkv.kv);
	dest->gkv.kv = NULL;

	return ret;
}

void metadata_free(struct agm_meta_data_gsl *metadata)
 {
	 if (metadata) {
         free(metadata->ckv.kv);
         metadata->ckv.kv = NULL;
         metadata->ckv.num_kvs = 0;
         free(metadata->gkv.kv);
         metadata->gkv.kv = NULL;
         metadata->gkv.num_kvs = 0;
	 }
 }
