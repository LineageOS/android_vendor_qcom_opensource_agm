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

#define LOG_TAG "AGMCallback"
#include "inc/AGMCallback.h"
#include <log/log.h>
namespace vendor {
namespace qti {
namespace hardware {
namespace AGMIPC {
namespace V1_0 {
namespace implementation {

Return<int32_t> AGMCallback::event_callback(uint32_t session_id,
                                const hidl_vec<AgmEventCbParams>& event_params,
                                uint64_t clbk_data) {
    ALOGV("%s called \n", __func__);
    ClntClbk *cl_clbk_data;
    cl_clbk_data = (ClntClbk *) clbk_data;
    struct agm_event_cb_params *event_params_l = NULL;
    event_params_l = (struct agm_event_cb_params*) calloc(1,
                     (sizeof(struct agm_event_cb_params) +
                     event_params.data()->event_payload_size));

    event_params_l->event_payload_size = event_params.data()->event_payload_size;
    event_params_l->event_id = event_params.data()->event_id;
    event_params_l->source_module_id = event_params.data()->source_module_id;
    int8_t *src = (int8_t *)event_params.data()->event_payload.data();
    int8_t *dst = (int8_t *)event_params_l->event_payload;
    memcpy(dst, src, event_params_l->event_payload_size);

    ALOGV("event_params payload_size %d", event_params_l->event_payload_size);
    agm_event_cb clbk_func = cl_clbk_data->get_clbk_func();
    clbk_func(session_id, event_params_l, cl_clbk_data->get_clnt_data() );
    return int32_t {};
}


}  // namespace implementation
}  // namespace V1_0
}  // namespace AGMIPC
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
