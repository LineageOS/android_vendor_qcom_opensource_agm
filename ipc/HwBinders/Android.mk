ifeq ($(call is-board-platform-in-list, sdm845 msmnile kona lahaina),true)


include $(call all-subdir-makefiles)








#----------------------------------------------------------------------------
#                 Common definitons
#----------------------------------------------------------------------------



#----------------------------------------------------------------------------
#             Make the Shared library (libqal)
#----------------------------------------------------------------------------

#LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc

#LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-audio/gsl

endif # is-board-platform-in-list
