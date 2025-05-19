pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = agm.pc
EXTRA_DIST = $(pkgconfig_DATA)

h_sources = ./inc/public/agm/agm_api.h \
            ./inc/public/agm/agm_list.h \
            ./inc/public/agm/utils.h \
            ./inc/private/agm/metadata.h \
            ./inc/private/agm/graph.h \
            ./inc/private/agm/session_obj.h \
            ./inc/private/agm/device.h

AM_CFLAGS = @SPF_CFLAGS@
AM_CFLAGS += -I $(srcdir)/inc/public
AM_CFLAGS += -I $(srcdir)/inc/private
AM_CFLAGS += @MMHEADERS_CFLAGS@
library_include_HEADERS = $(h_sources)
library_includedir = $(includedir)/agm

agm_sources = ./src/graph.c \
              ./src/graph_module.c \
              ./src/device.c \
              ./src/device_hw_ep.c \
              ./src/metadata.c \
              ./src/session_obj.c \
              ./src/agm.c \
              ./src/utils.c



lib_LTLIBRARIES = libagm.la
libagm_la_SOURCES = $(agm_sources)
libagm_la_LIBADD = -ltinyalsa -lar_osal -lar_gsl -lats -laudio_log_utils
# libagm_la_CFLAGS += -DCARD_STATE_UNSUPPORTED

libagm_la_LIBADD += -laudio_log_utils

libagm_la_CFLAGS = $(AM_CFLAGS) -DACDB_PATH=\"/etc/acdbdata/\" -DACDB_DELTA_FILE_PATH="/data/audio/delta"
if USE_SYSLOG
libagm_la_CFLAGS += -DAGM_USE_SYSLOG
endif
libagm_la_CFLAGS += -DAGM_MEMLOG_UNSUPPORTED -DCARD_STATE_UNSUPPORTED

libagm_la_CFLAGS += -D__unused=__attribute__\(\(__unused__\)\)
libagm_la_CFLAGS += @GLIB_CFLAGS@ -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat -include glib.h
libagm_la_LDFLAGS = -module -shared -avoid-version
