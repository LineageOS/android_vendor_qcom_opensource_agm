ifeq ($(call is-board-platform-in-list, msmnile),true)

MY_LOCAL_PATH := $(call my-dir)
include $(MY_LOCAL_PATH)/tinyalsa/Android.mk

endif # is-board-platform-in-list
