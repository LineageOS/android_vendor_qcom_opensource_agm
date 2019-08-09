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

include $(BUILD_EXECUTABLE)
endif
