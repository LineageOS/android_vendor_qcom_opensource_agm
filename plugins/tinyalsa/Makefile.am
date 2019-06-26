pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = agmplugin.pc
EXTRA_DIST = $(pkgconfig_DATA)

library_includedir = $(includedir)

AM_CFLAGS := -I $(top_srcdir)/include -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/agm/
AM_CFLAGS += -Wno-unused-parameter

lib_LTLIBRARIES      = libagm_pcm_plugin.la
libagm_pcm_plugin_la_SOURCES   = src/agm_pcm_plugin.c
libagm_pcm_plugin_la_CFLAGS := $(AM_CFLAGS)
libagm_pcm_plugin_la_LDFLAGS   = -ltinyalsa -lsndcardparser -lagmclient -avoid-version

lib_LTLIBRARIES      += libagm_mixer_plugin.la
libagm_mixer_plugin_la_SOURCES   = src/agm_mixer_plugin.c
libagm_mixer_plugin_la_CFLAGS := $(AM_CFLAGS)
libagm_mixer_plugin_la_LDFLAGS   = -ltinyalsa -lsndcardparser -lagmclient -avoid-version
