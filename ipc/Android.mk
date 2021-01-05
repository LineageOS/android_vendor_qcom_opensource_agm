ifeq ($(call is-board-platform-in-list, sdm845 msmnile kona lahaina taro),true)


include $(call all-subdir-makefiles)


endif # is-board-platform-in-list
