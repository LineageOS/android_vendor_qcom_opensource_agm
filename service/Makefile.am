pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = agm.pc
EXTRA_DIST = $(pkgconfig_DATA)


h_sources = ${top_srcdir}/inc/agm_api.h\
            ${top_srcdir}/inc/agm_list.h

AM_CFLAGS = -I ${top_srcdir}/inc  -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/gecko

library_include_HEADERS = $(h_sources)
library_includedir = $(includedir)/agm

agm_sources = ${top_srcdir}/src/graph.c \
              ${top_srcdir}/src/device.c \
              ${top_srcdir}/src/device_hw_ep.c \
              ${top_srcdir}/src/metadata.c \
              ${top_srcdir}/src/session_obj.c \
              ${top_srcdir}/src/agm.c

lib_LTLIBRARIES = libagm.la
libagm_la_SOURCES = $(agm_sources)
libagm_la_LIBADD = -ltinyalsa -lcasa_osal -lgsl
libagm_la_CFLAGS := $(AM_CFLAGS) -DACDB_PATH="/etc/acdbdata/MTP" -DACDB_DELTA_FILE_PATH="/data/audio/delta"
libagm_la_CFLAGS += -D__unused=__attribute__\(\(__unused__\)\)
libagm_la_CFLAGS += $(GLIB_CFLAGS) -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat -include glib.h
libagm_la_LDFLAGS = -module -shared -avoid-version
