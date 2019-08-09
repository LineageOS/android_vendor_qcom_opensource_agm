ifeq ($(call is-board-platform-in-list, sdm845 msmnile),true)


include $(call all-subdir-makefiles)


endif # is-board-platform-in-list
