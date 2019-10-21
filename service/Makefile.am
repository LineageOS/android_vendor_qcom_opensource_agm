pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = agm.pc
EXTRA_DIST = $(pkgconfig_DATA)

if BUILDSYSTEM_OPENWRT
h_sources = ./inc/agm_api.h \
            ./inc/agm_list.h \
            ./inc/utils.h

AM_CFLAGS = -I ./inc

library_include_HEADERS = $(h_sources)
library_includedir = $(includedir)/agm

agm_sources = ./src/graph.c \
              ./src/graph_module.c \
              ./src/device.c \
              ./src/device_hw_ep.c \
              ./src/metadata.c \
              ./src/session_obj.c \
              ./src/agm.c

else
h_sources = ${top_srcdir}/inc/agm_api.h \
            ${top_srcdir}/inc/agm_list.h \
            ${top_srcdir}/inc/utils.h

AM_CFLAGS = -I ${top_srcdir}/inc  -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/gecko
AM_CFLAGS += -DDYNAMIC_LOG_ENABLED

library_include_HEADERS = $(h_sources)
library_includedir = $(includedir)/agm

agm_sources = ${top_srcdir}/src/graph.c \
              ${top_srcdir}/src/graph_module.c \
              ${top_srcdir}/src/device.c \
              ${top_srcdir}/src/device_hw_ep.c \
              ${top_srcdir}/src/metadata.c \
              ${top_srcdir}/src/session_obj.c \
              ${top_srcdir}/src/agm.c

endif

lib_LTLIBRARIES = libagm.la
libagm_la_SOURCES = $(agm_sources)
libagm_la_LIBADD = -ltinyalsa -lcasa_osal -lgsl -lqts

if !BUILDSYSTEM_OPENWRT
libagm_la_LIBADD += -laudio_log_utils
endif

libagm_la_CFLAGS := $(AM_CFLAGS) -DACDB_PATH="/etc/acdbdata/MTP" -DACDB_DELTA_FILE_PATH="/data/audio/delta"
if BUILDSYSTEM_OPENWRT
libagm_la_LIBADD += -lglib-2.0
endif
libagm_la_CFLAGS += -D__unused=__attribute__\(\(__unused__\)\)
libagm_la_CFLAGS += $(GLIB_CFLAGS) -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat -include glib.h
libagm_la_LDFLAGS = -module -shared -avoid-version
