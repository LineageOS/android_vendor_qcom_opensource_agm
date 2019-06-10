LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	ipc_proxy_server.cpp

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libdl \
	libbinder \
	libutils

LOCAL_MODULE := libagmproxy
LOCAL_MODULE_TAGS := optional

LOCAL_COPY_HEADERS_TO   := mm-audio/qti-agm-server
LOCAL_COPY_HEADERS      := \
            ipc_interface.h

include $(BUILD_SHARED_LIBRARY)
include $(CLEAR_VARS)
LOCAL_MODULE := agmserver
LOCAL_SRC_FILES := \
    agm_server_daemon.cpp \
    agm_server_wrapper.cpp

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libdl \
	libbinder \
	libutils \
	libagmproxy

include $(BUILD_EXECUTABLE)
