pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = agmplugin.pc
EXTRA_DIST = $(pkgconfig_DATA)

library_includedir = $(includedir)

AM_CFLAGS = -Wno-unused-parameter
AM_CFLAGS +=  -I $(top_srcdir)/service/inc/public -I $(top_srcdir)/snd_parser/inc
AM_CFLAGS += @GLIB_CFLAGS@ -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat
AM_CFLAGS += -D__unused=__attribute__\(\(__unused__\)\) -DLINUX_ENABLED
#AM_CFLAGS += @SNDPARSER_CFLAGS@
AM_CFLAGS += -Wno-unused-parameter
AM_CFLAGS += -DDYNAMIC_LOG_ENABLED
AM_CFLAGS += -DAGM_USE_SYSLOG
AM_CFLAGS += @MMHEADERS_CFLAGS@

lib_LTLIBRARIES      = libagm_pcm_plugin.la
libagm_pcm_plugin_la_SOURCES   = src/agm_pcm_plugin.c
libagm_pcm_plugin_la_CFLAGS = $(AM_CFLAGS)
libagm_pcm_plugin_la_LDFLAGS  = -ltinyalsa -lsndcardparser -avoid-version -L$(top_builddir)/snd_parser/.libs  -shared
libagm_pcm_plugin_la_LDFLAGS +=   -laudio_log_utils
if AGM_NO_IPC
libagm_pcm_plugin_la_CFLAGS += -DAGM_NO_IPC
libagm_pcm_plugin_la_LDFLAGS += -lagm -L$(top_builddir)/service/.libs
else
libagm_pcm_plugin_la_LDFLAGS += -lagmclient -L$(top_builddir)/ipc/DBus/agm_client/.libs
endif

lib_LTLIBRARIES      += libagm_compress_plugin.la
libagm_compress_plugin_la_SOURCES   = src/agm_compress_plugin.c
libagm_compress_plugin_la_CFLAGS = $(AM_CFLAGS)
libagm_compress_plugin_la_LDFLAGS   = -ltinyalsa -ltinycompress -lsndcardparser -lpthread  -avoid-version -laudio_log_utils -L$(top_builddir)/snd_parser/.libs  -shared
if AGM_NO_IPC
libagm_compress_plugin_la_CFLAGS += -DAGM_NO_IPC
libagm_compress_plugin_la_LDFLAGS += -lagm -L$(top_builddir)/service/.libs
else
libagm_compress_plugin_la_LDFLAGS += -lagmclient -L$(top_builddir)/ipc/DBus/agm_client/.libs
endif


lib_LTLIBRARIES      += libagm_mixer_plugin.la
libagm_mixer_plugin_la_SOURCES   = src/agm_mixer_plugin.c
libagm_mixer_plugin_la_CFLAGS = $(AM_CFLAGS)
libagm_mixer_plugin_la_LDFLAGS  = -ltinyalsa -lsndcardparser -avoid-version -L$(top_builddir)/snd_parser/.libs -shared
libagm_mixer_plugin_la_LDFLAGS += -laudio_log_utils
if AGM_NO_IPC
libagm_mixer_plugin_la_CFLAGS += -DAGM_NO_IPC
libagm_mixer_plugin_la_LDFLAGS += -lagm -L$(top_builddir)/service/.libs
else
libagm_mixer_plugin_la_LDFLAGS += -lagmclient -L$(top_builddir)/ipc/DBus/agm_client/.libs
endif
