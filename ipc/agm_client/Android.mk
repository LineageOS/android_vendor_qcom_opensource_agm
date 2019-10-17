ifeq ($(call is-board-platform-in-list, sdm845),true)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := agmclient
LOCAL_SRC_FILES := \
    agm_client_wrapper.cpp \

LOCAL_C_INCLUDES += \
                    $(TARGET_OUT_HEADERS)/mm-audio/qti-agm-server/

LOCAL_SHARED_LIBRARIES := \
                          liblog \
                          libcutils \
                          libdl \
                          libbinder \
                          libutils \
                          libagmproxy

ifeq ($(strip $(AUDIO_FEATURE_ENABLED_DYNAMIC_LOG)), true)
      LOCAL_CFLAGS += -DDYNAMIC_LOG_ENABLED
      LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-audio/audio-log-utils
      LOCAL_C_INCLUDES += $(TOP)/external/expat/lib/expat.h
      LOCAL_SHARED_LIBRARIES += libaudio_log_utils \
                                libexpat
endif


include $(BUILD_EXECUTABLE)
endif
