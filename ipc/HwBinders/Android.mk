ifeq ($(call is-board-platform-in-list, sdm845 msmnile kona lahaina taro bengal monaco),true)
include $(call all-subdir-makefiles)
endif # is-board-platform-in-list
