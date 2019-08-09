ifeq ($(call is-board-platform-in-list, sdm845 msmnile),true)
ifneq ($(BUILD_TINY_ANDROID),true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(TOP)/external/tinyalsa/include $(TOP)/external/tinyalsa/include/snd-card-def/
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/

LOCAL_SRC_FILES := src/agm_pcm_plugin.c

LOCAL_MODULE := libagm_pcm_plugin
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
        libtinyalsa\
        libsndcardparser\
        libclient_ipc\
        libcutils
LOCAL_VENDOR_MODULE := true

include $(BUILD_SHARED_LIBRARY)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(TOP)/external/tinyalsa/include $(TOP)/external/tinyalsa/include/snd-card-def/
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/

LOCAL_SRC_FILES := src/agm_mixer_plugin.c

LOCAL_MODULE := libagm_mixer_plugin
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
        libtinyalsa\
        libsndcardparser\
        libclient_ipc\
        libcutils

LOCAL_VENDOR_MODULE := true
include $(BUILD_SHARED_LIBRARY)

endif
endif
