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

//#include "pch.h"
#include "agm_api.h"
#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_RATE_48KHZ 48000
#define FRAME_SIZE 4 /*2 channels 16 bit*/
#define BUFFER_DURATION_MS 20 /*in ms*/
#define NO_OF_FRAMES (BUFFER_DURATION_MS * SAMPLE_RATE_48KHZ)/1000
#define BUF_SIZE (NO_OF_FRAMES * FRAME_SIZE)
#define DURATION_IN_SECS 10
#define COUNT ((DURATION_IN_SECS * 1000)/BUFFER_DURATION_MS)

FILE *file_tx;
FILE *file_rx;
#define CAPTURE_FILE "/data/audio/agm_cap.raw"
#define PLAYBACK_FILE "/data/audio/agm_play.raw"

typedef int(*testcase)(void);

struct agm_key_value dev_rx_ckv[] = { {0xA5000000, 48000}, {0xA6000000, 16}};
struct agm_key_value dev_rx_gkv[] = {0xA2000000,0xA2000001};

struct agm_key_value dev_tx_ckv[] = { {0xA5000000, 48000}, {0xA6000000, 16}};
struct agm_key_value dev_tx_gkv[] = { 0xA3000000,0xA3000001};
struct agm_key_value stream_loopback_gkv[] = {0xA1000000, 0xA1000003};

struct agm_key_value dev3_ckv[] = { {1111,2222},{3333,4444} };
struct agm_key_value dev3_gkv[] = { {5555,6666},{7777,8888}, {9999,1000} };

struct agm_key_value stream_ckv[] = {{0xA5000000, 48000},{0xA6000000, 16}};
struct agm_key_value stream_gkv[] = {0xA1000000, 0xA1000001};

struct agm_key_value stream_tx_ckv[] = {{0xA5000000, 48000},{0xA6000000, 16}};
struct agm_key_value stream_tx_gkv[] = {0xA1000000, 0xA1000002};

struct agm_session_config stream_config = { 1, false, true, 0, 0 };
struct agm_session_config stream_tx_config = { 2, false, true, 0, 0 };
struct agm_media_config media_config = { 48000, 2, 16 };
struct agm_buffer_config buffer_config = { 4, BUF_SIZE};

uint32_t session_id = 1;
uint32_t session_id2 = 2;
uint32_t session_id_tx = 4;
uint32_t session_id_rx = 3;
uint32_t aif_id = 2;
uint32_t aif_id_tx = 0;
uint32_t aif_id2 = 1;
uint32_t aif_id3 = 33;

struct session_obj *sess_handle = NULL;
struct session_obj *sess_handle2 = NULL;


struct agm_meta_data stream_metadata = {
	{
		1, {stream_gkv},
	}, 
	{
		2, {stream_ckv},
	}
};

struct agm_meta_data stream_tx_metadata = {
	{
		1, {stream_tx_gkv},
	}, 
	{
		2, {stream_tx_ckv},
	}
};


struct agm_meta_data dev_rx_metadata = {
	{
		sizeof(dev_rx_gkv)/sizeof(struct agm_key_value), {dev_rx_gkv},
	},
	{
		sizeof(dev_rx_ckv)/sizeof(struct agm_key_value), {dev_rx_ckv},
	}
};

struct agm_meta_data dev_tx_metadata = {
	{
		1, {dev_tx_gkv},
	},
	{
		2, {dev_tx_ckv},
	}
};

struct agm_meta_data stream_loopback_metadata = {
	{
		1, {stream_loopback_gkv},
	},
	{
		2, {dev_tx_ckv},
	}
};

struct agm_meta_data dev3_metadata = {
	{
		3, {dev3_gkv},
	},
	{
		2, {dev3_ckv},
	}
};

int testcase_common_init(const char *caller) {
	printf("%s: init\n", caller);
	agm_init();
	return 0;
}

int testcase_common_deinit(const char *caller) {
	agm_deinit();
	printf("%s: deinit\n", caller);
	return 0;
}

int test_stream_open_without_aif_connected(void) {
	int ret = 0;

	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}
	// device config
	ret = agm_audio_intf_set_media_config(aif_id, &media_config);
	if (ret) {
		goto fail;
	}

	// device metadata
	ret = agm_audio_intf_set_metadata(aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	agm_session_set_metadata(session_id, &stream_metadata);
	if (ret) {
		goto fail;
	}


	// stream open with no device connected state
	ret = agm_session_open(session_id, &sess_handle);
	if (ret == 0) {
		goto fail;
	}
	else {
		//overwrite ret so test passess
		ret = 0;
	}

	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;
}

int test_device_get_aif_list() {
	int ret = 0;
	size_t num_aif_info = 0;
	struct aif_info *aifinfo;
        int i = 0;

	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}

	ret = agm_get_aif_info_list(aifinfo, &num_aif_info);
	if (ret) {
		goto fail;
	}
        if (num_aif_info > 0 ) {
            aifinfo = calloc(num_aif_info, sizeof(struct aif_info));          
        } else {
            ret = -1;
            goto fail;
        }
        ret = agm_get_aif_info_list(aifinfo, &num_aif_info);
        if (ret) {
            goto fail;
        }
        for (i = 0; i < num_aif_info; i++)
            printf("aif %d %s %d\n", i, aifinfo[i].aif_name, aifinfo[i].dir);

	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;

}

int test_stream_sssd(void) {
	int ret = 0;
	char buff[BUF_SIZE] = {0};
	int i = 0;
	size_t count = 0;

	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}
	// device config
	ret = agm_audio_intf_set_media_config(aif_id, &media_config);
	if (ret) {
		goto fail;
	}

	// device metadata
	ret = agm_audio_intf_set_metadata(aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream-device metadata
	//ret = agm_session_audio_inf_set_metadata(session_id, aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	agm_session_set_metadata(session_id, &stream_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_open(session_id, &sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_config(sess_handle, &stream_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}

	ret = agm_session_prepare(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle);
	if (ret) {
		goto fail;
	}

        /* TODO : Add agm_session_wrtie API */

	ret = agm_session_stop(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_close(sess_handle);
	if (ret) {
		goto fail;
	}

	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;
}

int test_stream_sssd_tx(void) {
	int ret = 0;
	char buff[BUF_SIZE] = {0};
	int i,j = 0;
	size_t count = 0;

        file_tx = fopen(CAPTURE_FILE, "w+");
        if (!file_tx) {
            printf("Cannot open file %s\n", CAPTURE_FILE);
        }

	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}
	// device config
	ret = agm_audio_intf_set_media_config(aif_id_tx, &media_config);
	if (ret) {
		goto fail;
	}

	// device metadata
	ret = agm_audio_intf_set_metadata(aif_id_tx, &dev_tx_metadata);
	if (ret) {
		goto fail;
	}

	// stream-device metadata
	//ret = agm_session_audio_inf_set_metadata(session_id, aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	agm_session_set_metadata(session_id2, &stream_tx_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id2, aif_id_tx, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_open(session_id2, &sess_handle);
	if (ret) {
		goto fail;
	}

        ret = agm_session_set_config(sess_handle, &stream_tx_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}

	ret = agm_session_prepare(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle);
	if (ret) {
		goto fail;
	}

	for(i = 0; i < COUNT ; i++) {
		ret = agm_session_read(sess_handle, buff, BUF_SIZE);
		if (ret) {
			printf("%s: Error:%d, session write  failed\n", __func__, ret);
			goto fail;
		}
                if (file_tx) {
                    fwrite(buff, 1, BUF_SIZE, file_tx);
                }

		count = agm_get_hw_processed_buff_cnt(sess_handle, RX);
		if (!count) {
			printf("%s: Error:%d, getting session buf count failed\n", __func__, ret);
			goto fail;
		}
                
	}

        if (file_tx)
            fclose(file_tx);
	ret = agm_session_stop(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_close(sess_handle);
	if (ret) {
		goto fail;
	}

	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;
}


int test_stream_pause_resume(void) {
	int ret = 0;

	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}
	// device config
	ret = agm_audio_intf_set_media_config(aif_id, &media_config);
	if (ret) {
		goto fail;
	}

	// device metadata
	ret = agm_audio_intf_set_metadata(aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	agm_session_set_metadata(session_id, &stream_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_open(session_id, &sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_config(sess_handle, &stream_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}

	ret = agm_session_prepare(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle);
	if (ret) {
		goto fail;
	}
	ret = agm_session_pause(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_resume(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_stop(sess_handle);
	if (ret) {
		goto fail;
	}
	
	ret = agm_session_pause(sess_handle);
	if (ret == 0) {
		goto fail;
	}
	else {
		//overwrite ret so test passess
		ret = 0;
	}

	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;
}

int test_stream_open_with_same_aif_twice(void) {
	int ret = 0;

	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}
	// device config
	ret = agm_audio_intf_set_media_config(aif_id, &media_config);
	if (ret) {
		goto fail;
	}

	// device metadata
	ret = agm_audio_intf_set_metadata(aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	agm_session_set_metadata(session_id, &stream_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_open(session_id, &sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_config(sess_handle, &stream_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}

	ret = agm_session_prepare(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, true);
	if (ret == 0) {
		goto fail;
	}
	else {
		ret = 0;
	}

	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;
}

int test_stream_sssd_deviceswitch(void) {
	int ret = 0;

	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}
	// device config
	ret = agm_audio_intf_set_media_config(aif_id, &media_config);
	if (ret) {
		goto fail;
	}

	// device metadata
	ret = agm_audio_intf_set_metadata(aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	agm_session_set_metadata(session_id, &stream_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_open(session_id, &sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_config(sess_handle, &stream_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}

	ret = agm_session_prepare(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle);
	if (ret) {
		goto fail;
	}

	// disconnect device 1
	ret = agm_session_audio_inf_connect(session_id, aif_id, false);
	if (ret) {
		goto fail;
	}

	// device2 config
	ret = agm_audio_intf_set_media_config(aif_id2, &media_config);
	if (ret) {
		goto fail;
	}

	// device2 metadata
	ret = agm_audio_intf_set_metadata(aif_id2, &dev_tx_metadata);
	if (ret) {
		goto fail;
	}

	// stream device metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id2, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id2, true);
	if (ret) {
		goto fail;
	}


	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;
}

int test_stream_ssmd(void) {
	int ret = 0;

	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}
	// device config
	ret = agm_audio_intf_set_media_config(aif_id, &media_config);
	if (ret) {
		goto fail;
	}

	// device metadata
	ret = agm_audio_intf_set_metadata(aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	agm_session_set_metadata(session_id, &stream_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_open(session_id, &sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_config(sess_handle, &stream_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}

	ret = agm_session_prepare(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle);
	if (ret) {
		goto fail;
	}

	// device2 config
	ret = agm_audio_intf_set_media_config(aif_id2, &media_config);
	if (ret) {
		goto fail;
	}

	// device2 metadata
	ret = agm_audio_intf_set_metadata(aif_id2, &dev_tx_metadata);
	if (ret) {
		goto fail;
	}

	// stream device metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id2, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id2, true);
	if (ret) {
		goto fail;
	}

	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;
}

int test_stream_ssmd_teardown_first_device(void) {
	int ret = 0;

	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}

	// device config
	ret = agm_audio_intf_set_media_config(aif_id, &media_config);
	if (ret) {
		goto fail;
	}

	// device metadata
	ret = agm_audio_intf_set_metadata(aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	agm_session_set_metadata(session_id, &stream_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, true);
	if (ret) {
		goto fail;
	}

	
	ret = agm_session_open(session_id, &sess_handle);
	if (ret) {
		goto fail;
	}


	ret = agm_session_set_config(sess_handle, &stream_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}

	ret = agm_session_prepare(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle);
	if (ret) {
		goto fail;
	}

	// device2 config
	ret = agm_audio_intf_set_media_config(aif_id2, &media_config);
	if (ret) {
		goto fail;
	}

	// device2 metadata
	ret = agm_audio_intf_set_metadata(aif_id2, &dev_tx_metadata);
	if (ret) {
		goto fail;
	}

	// stream device metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id2, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id2, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, false);
	if (ret) {
		goto fail;
	}

	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;
}

int test_stream_ssmd_teardown_both_devices_resetup_firstdevice(void) {
	int ret = 0;

	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}
	// device config
	ret = agm_audio_intf_set_media_config(aif_id, &media_config);
	if (ret) {
		goto fail;
	}

	// device metadata
	ret = agm_audio_intf_set_metadata(aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	agm_session_set_metadata(session_id, &stream_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_open(session_id, &sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_config(sess_handle, &stream_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}

	ret = agm_session_prepare(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle);
	if (ret) {
		goto fail;
	}

	// device2 config
	ret = agm_audio_intf_set_media_config(aif_id2, &media_config);
	if (ret) {
		goto fail;
	}

	// device2 metadata
	ret = agm_audio_intf_set_metadata(aif_id2, &dev_tx_metadata);
	if (ret) {
		goto fail;
	}

	// stream device metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id2, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id2, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, false);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id2, false);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, true);
	if (ret) {
		goto fail;
	}

	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;
}

int test_stream_ssmd_deviceswitch_second_device(void) {
	int ret = 0;

	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}
	// device config
	ret = agm_audio_intf_set_media_config(aif_id, &media_config);
	if (ret) {
		goto fail;
	}

	// device metadata
	ret = agm_audio_intf_set_metadata(aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	agm_session_set_metadata(session_id, &stream_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_open(session_id, &sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_config(sess_handle, &stream_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}

	ret = agm_session_prepare(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle);
	if (ret) {
		goto fail;
	}

	// device2 config
	ret = agm_audio_intf_set_media_config(aif_id2, &media_config);
	if (ret) {
		goto fail;
	}

	// device2 metadata
	ret = agm_audio_intf_set_metadata(aif_id2, &dev_tx_metadata);
	if (ret) {
		goto fail;
	}

	// stream device metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id2, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id2, true);
	if (ret) {
		goto fail;
	}

	//tear down second device
	ret = agm_session_audio_inf_connect(session_id, aif_id2, false);
	if (ret) {
		goto fail;
	}

	// device3 config
	ret = agm_audio_intf_set_media_config(aif_id3, &media_config);
	if (ret) {
		goto fail;
	}

	// device3 metadata
	ret = agm_audio_intf_set_metadata(aif_id3, &dev3_metadata);
	if (ret) {
		goto fail;
	}

	// stream device3 metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id3, &dev3_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id3, true);
	if (ret) {
		goto fail;
	}

	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;
}

int test_stream_deint_with_msmd(void) {
	int ret = 0;

	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}
	// device config
	ret = agm_audio_intf_set_media_config(aif_id, &media_config);
	if (ret) {
		goto fail;
	}

	// device metadata
	ret = agm_audio_intf_set_metadata(aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
	agm_session_set_metadata(session_id, &stream_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id, true);
	if (ret) {
		goto fail;
	}

	// device2 config
	ret = agm_audio_intf_set_media_config(aif_id2, &media_config);
	if (ret) {
		goto fail;
	}

	// device2 metadata
	ret = agm_audio_intf_set_metadata(aif_id2, &dev_tx_metadata);
	if (ret) {
		goto fail;
	}

	// stream device metadata
	ret = agm_session_audio_inf_set_metadata(session_id, aif_id2, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id, aif_id2, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_open(session_id, &sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_config(sess_handle, &stream_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}


	ret = agm_session_prepare(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle);
	if (ret) {
		goto fail;
	}

	//open second session

	ret = agm_session_audio_inf_connect(session_id2, aif_id2, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_open(session_id2, &sess_handle2);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_config(sess_handle2, &stream_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}

	ret = agm_session_prepare(sess_handle2);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle2);
	if (ret) {
		goto fail;
	}

	ret = agm_deinit();
	if (ret) {
		goto fail;
	}

	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;
}

int test_capture_sess_loopback()
{
	int ret = 0;
	ret = testcase_common_init(__func__);
	if (ret) {
		goto fail;
	}

	//open capture session
	// device_tx config
	ret = agm_audio_intf_set_media_config(aif_id_tx, &media_config);
	if (ret) {
		goto fail;
	}

	// device_tx metadata
	ret = agm_audio_intf_set_metadata(aif_id_tx, &dev_tx_metadata);
	if (ret) {
		goto fail;
	}

//	// stream device metadata
//	ret = agm_session_audio_inf_set_metadata(session_id2, aif_id2, &dev_rx_metadata);
//	if (ret) {
//		goto fail;
//	}

	// stream metadata
	agm_session_set_metadata(session_id_tx, &stream_loopback_metadata);
	if (ret) {
		goto fail;
	}

	ret = agm_session_audio_inf_connect(session_id_tx, aif_id_tx, true);
	if (ret) {
		goto fail;
	}


	//set playback session
	// device config
	ret = agm_audio_intf_set_media_config(aif_id, &media_config);
	if (ret) {
		goto fail;
	}

	// device metadata
	ret = agm_audio_intf_set_metadata(aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}

	// stream metadata
//	ret = agm_session_audio_inf_set_metadata(session_id, aif_id, &dev_rx_metadata);
	if (ret) {
		goto fail;
	}


	ret = agm_session_audio_inf_connect(session_id_rx, aif_id, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_loopback(session_id_tx, session_id_rx, true);
	if (ret) {
		goto fail;
	}

	ret = agm_session_open(session_id_tx, &sess_handle2);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_config(sess_handle2, &stream_tx_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}

	ret = agm_session_open(session_id_rx, &sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_config(sess_handle, &stream_config, &media_config, &buffer_config);
	if (ret) {
		goto fail;
	}

	ret = agm_session_prepare(sess_handle2);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle2);
	if (ret) {
		goto fail;
	}

	ret = agm_session_prepare(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_start(sess_handle);
	if (ret) {
		goto fail;
	}

 
        sleep(20);


	ret = agm_session_close(sess_handle);
	if (ret) {
		goto fail;
	}

	ret = agm_session_close(sess_handle2);
	if (ret) {
		goto fail;
	}

	ret = agm_session_set_loopback(session_id_tx, 0, false);
	if (ret) {
		goto fail;
	}

	printf("TEST PASS: %s()\n", __func__);
	goto done;

fail:
	printf("TEST FAIL: %s()\n", __func__);
	goto done;

done:
	testcase_common_deinit(__func__);
	return ret;
}


int main() {
	int ret = 0;
	int i = 0;

	testcase testcases[] = {
                                test_device_get_aif_list,
                                test_stream_sssd,
                                test_capture_sess_loopback,
				test_stream_sssd_tx,
				test_stream_sssd_deviceswitch,
				test_stream_ssmd,
				test_stream_ssmd_teardown_first_device,
				test_stream_ssmd_deviceswitch_second_device,
				test_stream_ssmd_teardown_both_devices_resetup_firstdevice,
				test_stream_pause_resume,
				/*adverserial test cases*/
				test_stream_open_without_aif_connected,
				test_stream_open_with_same_aif_twice,
				test_stream_deint_with_msmd,
	};

	int testcount  = sizeof(testcases)/sizeof(testcase);
        testcount = 4;
	int failed_count = 0;
        
	for (i = 0; i < testcount; i++) {
		printf("************* Start TestCase:%d*************\n", i+1);
		ret = testcases[i]();
		if (ret) {
			printf("Failed @ testcase no :%d\n", i+1);
			failed_count++;
		}
		printf("************* End TestCase:%d*************\n", i+1);
		printf("\n\n");
	}
	
	printf("\n\n");
	printf("*************TEST REPORT*************\n");
	printf("RAN:           %d/%d\n", i, testcount);
	printf("SUCCESSESFULL: %d\n", testcount- failed_count);
	printf("FAILED:        %d\n", failed_count);
	printf("*************************************\n\n\n\n");
	return 0;
}
