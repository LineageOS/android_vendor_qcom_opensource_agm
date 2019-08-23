ifeq ($(call is-board-platform-in-list, sdm845 msmnile),true)

ifneq ($(BUILD_TINY_ANDROID),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_USE_VNDK := true

#----------------------------------------------------------------------------
#                 Common definitons
#----------------------------------------------------------------------------

agm-def += -D_ANDROID_

#----------------------------------------------------------------------------
#             Make the Shared library (libagm)
#----------------------------------------------------------------------------

LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc

LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/casa/casa_osal
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/casa/gsl
LOCAL_C_INCLUDES += $(TOP)/external/tinyalsa/include

LOCAL_CFLAGS     := $(agm-def)
LOCAL_CFLAGS += -Wno-tautological-compare
LOCAL_CFLAGS += -Wno-macro-redefined
LOCAL_CFLAGS += -D_GNU_SOURCE -DACDB_PATH="/vendor/etc/acdbdata/MTP" -DACDB_DELTA_FILE_PATH="/data/vendor/audio/acdbdata/delta"

LOCAL_SRC_FILES  := src/agm.c\
	src/graph.c\
	src/graph_module.c\
	src/metadata.c\
	src/session_obj.c\
	src/device.c \
        src/device_hw_ep.c

LOCAL_MODULE               := libagm
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_TAGS          := optional

LOCAL_HEADER_LIBRARIES := libgecko-headers \
                          libcasa-acdbdata
LOCAL_SHARED_LIBRARIES := \
	libcasa-gsl\
	liblog\
	liblx-osal\
	libaudioroute\
	libtinyalsa

LOCAL_COPY_HEADERS_TO   := mm-audio/agm
LOCAL_COPY_HEADERS      := inc/agm_api.h

LOCAL_VENDOR_MODULE := true

include $(BUILD_SHARED_LIBRARY)

endif # BUILD_TINY_ANDROID
endif # is-board-platform-in-list
