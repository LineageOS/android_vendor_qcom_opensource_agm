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

struct agm_meta_data* metadata_merge(int num, ...)
{
	int total_gkv = 0;
	int total_ckv = 0;
	struct agm_key_value *gkv_offset;
	struct agm_key_value *ckv_offset;
	struct agm_meta_data *temp, *merged = NULL;

	va_list valist;
	int i = 0;

	va_start(valist, num);
	for (i = 0; i < num; i++) {
		temp = va_arg(valist, struct agm_meta_data*);
		if (temp) {
			total_gkv += temp->gkv.num_kvs;
			total_ckv += temp->ckv.num_kvs;
		}
	}
	va_end(valist);

	merged = calloc(1, sizeof(struct agm_meta_data));
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
		temp = va_arg(valist, struct agm_meta_data*);
		if (temp) {
			memcpy(gkv_offset, temp->gkv.kv, temp->gkv.num_kvs * sizeof(struct agm_key_value));
			gkv_offset += temp->gkv.num_kvs;
			memcpy(ckv_offset, temp->ckv.kv, temp->ckv.num_kvs * sizeof(struct agm_key_value));
			ckv_offset += temp->ckv.num_kvs;
		}
	}
	va_end(valist);

	return merged;
}

int metadata_copy(struct agm_meta_data *dest, struct agm_meta_data *src)
{
	int ret = 0;

	//validate src metadata
	if (src->ckv.num_kvs &&
		(!src->ckv.kv)) {
		AGM_LOGE("%s: Invalid input CKV", __func__);
		return -EINVAL;
	}

	if (src->gkv.num_kvs &&
		(!src->gkv.kv)) {
		AGM_LOGE("%s: Invalid input GKV", __func__);
		return -EINVAL;
	}

	// free cached metadata first
	free(dest->ckv.kv);
	dest->ckv.kv = NULL;
	free(dest->gkv.kv);
	dest->gkv.kv = NULL;

	// set new no of kvs
	dest->ckv.num_kvs = src->ckv.num_kvs;
	dest->gkv.num_kvs = src->gkv.num_kvs;

	//allocate new ckv array
	dest->ckv.kv = calloc(src->ckv.num_kvs, sizeof(struct agm_key_value));
	if (!dest->ckv.kv) {
		printf("%s No memory to allocate CKV\n", __func__);
		ret = -ENOMEM;
		goto error;
	}
	memcpy(dest->ckv.kv, src->ckv.kv, src->ckv.num_kvs * sizeof(struct agm_key_value));

	//allocate new gkv array
	dest->gkv.kv = calloc(src->gkv.num_kvs, sizeof(struct agm_key_value));
	if (!dest->gkv.kv) {
		printf("%s No memory to allocate GKV \n", __func__);
		ret = -ENOMEM;
		goto error;
	}
	memcpy(dest->gkv.kv, src->gkv.kv, src->gkv.num_kvs * sizeof(struct agm_key_value));

	return ret;

error:
	free(dest->ckv.kv);
	dest->ckv.kv = NULL;
	free(dest->gkv.kv);
	dest->gkv.kv = NULL;

	return ret;
}

void metadata_free(struct agm_meta_data *metadata)
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
