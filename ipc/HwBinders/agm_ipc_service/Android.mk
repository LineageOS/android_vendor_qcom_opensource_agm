LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/agm
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/agm/ipc/HwBinders/agm_ipc_client/
LOCAL_MODULE := vendor.qti.hardware.AGMIPC@1.0-impl
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS += -v
LOCAL_SRC_FILES := \
    src/agm_server_wrapper.cpp

LOCAL_SHARED_LIBRARIES := \
    libhidlbase \
    libhidltransport \
    libutils \
    liblog \
    libcutils \
    libhardware \
    libbase \
    vendor.qti.hardware.AGMIPC@1.0 \
    libagm

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/include/mm-audio/agm
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/agm/ipc/HwBinders/agm_ipc_client/
LOCAL_MODULE := vendor.qti.hardware.AGMIPC@1.0-service
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_OWNER := qti
LOCAL_SRC_FILES := \
    src/service.cpp \

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libdl \
    libbase \
    libutils \
    libhardware \
    libhidlbase \
    libhidltransport \
    vendor.qti.hardware.AGMIPC@1.0 \
    libhwbinder \
    vendor.qti.hardware.AGMIPC@1.0-impl \
    libagm

include $(BUILD_EXECUTABLE)
