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
#define LOG_TAG "AGM"

#include<errno.h>

#include "utils.h"

/* ERROR STRING */
#define CASA_EOK_STR          "CASA_EOK"
#define CASA_EFAILED_STR      "CASA_EFAILED"
#define CASA_EBADPARAM_STR    "CASA_EBADPARAM"
#define CASA_EUNSUPPORTED_STR "CASA_EUNSUPPORTED"
#define CASA_EVERSION_STR     "CASA_EVERSION"
#define CASA_EUNEXPECTED_STR  "CASA_EUNEXPECTED"
#define CASA_EPANIC_STR       "CASA_EPANIC"
#define CASA_ENORESOURCE_STR  "CASA_ENORESOURCE"
#define CASA_EHANDLE_STR      "CASA_EHANDLE"
#define CASA_EALREADY_STR     "CASA_EALREADY"
#define CASA_ENOTREADY_STR    "CASA_ENOTREADY"
#define CASA_EPENDING_STR     "CASA_EPENDING"
#define CASA_EBUSY_STR        "CASA_EBUSY"
#define CASA_EABORTED_STR     "CASA_EABORTED"
#define CASA_ECONTINUE_STR    "CASA_ECONTINUE"
#define CASA_EIMMEDIATE_STR   "CASA_EIMMEDIATE"
#define CASA_ENOTIMPL_STR     "CASA_ENOTIMPL"
#define CASA_ENEEDMORE_STR    "CASA_ENEEDMORE"
#define CASA_ENOMEMORY_STR    "CASA_ENOMEMORY"
#define CASA_ENOTEXIST_STR    "CASA_ENOTEXIST"
#define CASA_ETERMINATED_STR  "CASA_ETERMINATED"
#define CASA_ETIMEOUT_STR  "CASA_ETIMEOUT"
#define CASA_ERR_MAX_STR      "CASA_ERR_MAX"

/*
 *osal layer does not define a max error code hence assigning it
 *from based on the latest header, need to revisit each time
 *a new error code is added
 */
#define CASA_ERR_MAX CASA_ETIMEOUT + 1

struct casa_err_code {
    int  lnx_err_code;
    char *casa_err_str;
};

static struct casa_err_code casa_err_code_info[CASA_ERR_MAX+1] = {
    { 0, CASA_EOK_STR},
    { -ENOTRECOVERABLE, CASA_EFAILED_STR},
    { -EINVAL, CASA_EBADPARAM_STR},
    { -EOPNOTSUPP, CASA_EUNSUPPORTED_STR},
    { -ENOPROTOOPT, CASA_EVERSION_STR},
    { -ENOTRECOVERABLE, CASA_EUNEXPECTED_STR},
    { -ENOTRECOVERABLE, CASA_EPANIC_STR},
    { -ENOSPC, CASA_ENORESOURCE_STR},
    { -EBADR, CASA_EHANDLE_STR},
    { -EALREADY, CASA_EALREADY_STR},
    { -EPERM, CASA_ENOTREADY_STR},
    { -EINPROGRESS, CASA_EPENDING_STR},
    { -EBUSY, CASA_EBUSY_STR},
    { -ECANCELED, CASA_EABORTED_STR},
    { -EAGAIN, CASA_ECONTINUE_STR},
    { -EAGAIN, CASA_EIMMEDIATE_STR},
    { -EAGAIN, CASA_ENOTIMPL_STR},
    { -ENODATA, CASA_ENEEDMORE_STR},
    { -ENOMEM, CASA_ENOMEMORY_STR},
    { -ENODEV, CASA_ENOTEXIST_STR},
    { -ENODEV, CASA_ETERMINATED_STR},
    { -ETIMEDOUT, CASA_ETIMEOUT_STR},
    { -EADV, CASA_ERR_MAX_STR},
};

int cass_err_get_lnx_err_code(uint32_t error)
{
    if (error > CASA_ERR_MAX)
        return casa_err_code_info[CASA_ERR_MAX].lnx_err_code;
    else
        return casa_err_code_info[error].lnx_err_code;
}

char *casa_err_get_err_str(uint32_t error)
{
    if (error > CASA_ERR_MAX)
        return casa_err_code_info[CASA_ERR_MAX].casa_err_str;
    else
        return casa_err_code_info[error].casa_err_str;
}
