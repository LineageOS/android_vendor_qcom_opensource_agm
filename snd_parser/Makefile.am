pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sndparser.pc
EXTRA_DIST = $(pkgconfig_DATA)

AUTOMAKE_OPTIONS = subdir-objects

h_sources  = inc/snd-card-def.h

lib_include_HEADERS = $(h_sources)
lib_includedir = $(includedir)/sndparser/

AM_CFLAGS = -I $(srcdir)/inc -I $(top_srcdir)/service/inc/public

AM_CFLAGS += -Wno-unused-parameter
AM_CFLAGS += -DCARD_DEF_FILE=\"/etc/card-defs.xml\"

lib_LTLIBRARIES      = libsndcardparser.la
libsndcardparser_la_SOURCES   = src/snd-card-parser.c
libsndcardparser_la_LIBADD = @GLIB_LIBS@ -lexpat -lpthread
libsndcardparser_la_CFLAGS = $(AM_CFLAGS)
libsndcardparser_la_CFLAGS += @GLIB_CFLAGS@ -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat -include glib.h
libsndcardparser_la_LDFLAGS   = -avoid-version -shared
libsndcardparser_la_list   = $(srcdir)/configs/qcm2290-mtp/card-defs.xml
#install xml files under /etc
root_etcdir = "/etc"
root_etc_SCRIPTS = $(libsndcardparser_la_list)
install-data-hook:
	chmod  go+r $(DESTDIR)$(root_etcdir)/card-defs.xml
