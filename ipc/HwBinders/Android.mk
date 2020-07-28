ifeq ($(call is-board-platform-in-list, sdm845 msmnile kona lahaina taro bengal),true)

include $(call all-subdir-makefiles)








#----------------------------------------------------------------------------
#                 Common definitons
#----------------------------------------------------------------------------



#LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc

#LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-audio/gsl

endif # is-board-platform-in-list
