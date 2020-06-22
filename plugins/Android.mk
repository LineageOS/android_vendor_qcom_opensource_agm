ifeq ($(call is-board-platform-in-list, msmnile kona lahaina),true)

MY_LOCAL_PATH := $(call my-dir)
include $(MY_LOCAL_PATH)/tinyalsa/Android.mk
include $(MY_LOCAL_PATH)/tinyalsa/test/Android.mk

endif # is-board-platform-in-list
