ifeq ($(call is-board-platform-in-list, sdm845 msmnile kona lahaina),true)
ifneq ($(BUILD_TINY_ANDROID),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_USE_VNDK := true
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/tinyalsa/include
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/snd-card-parser/
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/ar/ar_osal
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/agm/
LOCAL_C_INCLUDES += $(call include-path-for, agm) \

LOCAL_SRC_FILES := src/agm_pcm_plugin.c

LOCAL_MODULE := libagm_pcm_plugin
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
        libqti-tinyalsa \
        libsndcardparser \
        libagmclient \
        libutils \
        libcutils \
        liblog

LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS += -Wall

ifeq ($(strip $(AUDIO_FEATURE_ENABLED_DYNAMIC_LOG)), true)
      LOCAL_CFLAGS += -DDYNAMIC_LOG_ENABLED
      LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-audio/audio-log-utils
      LOCAL_C_INCLUDES += $(TOP)/external/expat/lib/expat.h
      LOCAL_SHARED_LIBRARIES += libaudio_log_utils \
                                libexpat
endif

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_USE_VNDK := true
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/tinyalsa/include
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/snd-card-parser/
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/ar/ar_osal
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/agm/
LOCAL_C_INCLUDES += $(call include-path-for, agm) \

LOCAL_SRC_FILES := src/agm_mixer_plugin.c

LOCAL_MODULE := libagm_mixer_plugin
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
        libqti-tinyalsa \
        libsndcardparser \
        libagmclient \
        libcutils \
        libutils \
        liblog

LOCAL_VENDOR_MODULE := true

ifeq ($(strip $(AUDIO_FEATURE_ENABLED_DYNAMIC_LOG)), true)
      LOCAL_CFLAGS += -DDYNAMIC_LOG_ENABLED
      LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-audio/audio-log-utils
      LOCAL_C_INCLUDES += $(TOP)/external/expat/lib/expat.h
      LOCAL_SHARED_LIBRARIES += libaudio_log_utils \
                                libexpat
endif

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_USE_VNDK := true
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/tinyalsa/include
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/tinycompress/include
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/agm/
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/snd-card-parser/
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/ar/ar_osal
LOCAL_C_INCLUDES += $(call include-path-for, agm) \

LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_SRC_FILES := src/agm_compress_plugin.c

LOCAL_MODULE := libagm_compress_plugin
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
        libqti-tinyalsa \
        libqti-tinycompress \
        libsndcardparser \
        libagmclient \
        libutils \
        libcutils \
        liblog

LOCAL_VENDOR_MODULE := true

ifeq ($(strip $(AUDIO_FEATURE_ENABLED_DYNAMIC_LOG)), true)
      LOCAL_CFLAGS += -DDYNAMIC_LOG_ENABLED
      LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-audio/audio-log-utils
      LOCAL_C_INCLUDES += $(TOP)/external/expat/lib/expat.h
      LOCAL_SHARED_LIBRARIES += libaudio_log_utils \
                                libexpat
endif

include $(BUILD_SHARED_LIBRARY)

endif
endif
