ifeq ($(call is-board-platform-in-list, sdm845 msmnile kona lahaina),true)
ifneq ($(BUILD_TINY_ANDROID),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(TOP)/external/tinyalsa/include
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/
LOCAL_CFLAGS += -Wno-unused-parameter -Wno-unused-result

LOCAL_SRC_FILES := agmmixer.c

LOCAL_MODULE := libagmmixer
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_TAGS := optional

LOCAL_HEADER_LIBRARIES := libar-acdbdata

LOCAL_SHARED_LIBRARIES := \
        libtinyalsa

LOCAL_VENDOR_MODULE := true

include $(BUILD_SHARED_LIBRARY)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(TOP)/external/tinyalsa/include
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/
LOCAL_CFLAGS += -Wno-unused-parameter -Wno-unused-result
LOCAL_SRC_FILES := agmplay.c

LOCAL_MODULE := agmplay
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_TAGS := optional

LOCAL_HEADER_LIBRARIES := libar-acdbdata
LOCAL_SHARED_LIBRARIES := \
        libtinyalsa\
        libagmmixer

LOCAL_VENDOR_MODULE := true
include $(BUILD_EXECUTABLE)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(TOP)/external/tinyalsa/include
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/
LOCAL_CFLAGS += -Wno-unused-parameter -Wno-unused-result

LOCAL_SRC_FILES := agmcap.c

LOCAL_MODULE := agmcap
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_TAGS := optional

LOCAL_HEADER_LIBRARIES := libar-acdbdata
LOCAL_SHARED_LIBRARIES := \
        libtinyalsa\
        libagmmixer

LOCAL_VENDOR_MODULE := true
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(TOP)/external/tinyalsa/include
LOCAL_C_INCLUDES += $(TOP)/external/tinycompress/include
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/
LOCAL_CFLAGS += -Wno-unused-parameter -Wno-unused-result

LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_SRC_FILES := agmcompressplay.c

LOCAL_HEADER_LIBRARIES := libar-acdbdata
LOCAL_MODULE := agmcompressplay
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
        libtinyalsa\
        libtinycompress\
        libagmmixer

LOCAL_VENDOR_MODULE := true
include $(BUILD_EXECUTABLE)
endif
endif
