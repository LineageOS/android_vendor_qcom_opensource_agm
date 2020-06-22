ifeq ($(call is-board-platform-in-list, msmnile kona lahaina),true)

ifneq ($(BUILD_TINY_ANDROID),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_USE_VNDK := true

#----------------------------------------------------------------------------
#             Make the Shared library (libsndcardparser)
#----------------------------------------------------------------------------

LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/

LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_CFLAGS += -DCARD_DEF_FILE=\"/vendor/etc/card-defs.xml\"

LOCAL_SRC_FILES  := src/snd-card-parser.c

LOCAL_MODULE               := libsndcardparser
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_TAGS          := optional

LOCAL_SHARED_LIBRARIES := \
	libexpat \
        libcutils

LOCAL_COPY_HEADERS_TO   := mm-audio/snd-card-parser
LOCAL_COPY_HEADERS      := inc/snd-card-def.h

LOCAL_VENDOR_MODULE := true

include $(BUILD_SHARED_LIBRARY)

endif # BUILD_TINY_ANDROID
endif # is-board-platform-in-list
