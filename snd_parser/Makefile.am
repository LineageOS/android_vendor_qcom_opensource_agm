pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sndparser.pc
EXTRA_DIST = $(pkgconfig_DATA)

AUTOMAKE_OPTIONS = subdir-objects

h_sources  = inc/snd-card-def.h

lib_include_HEADERS = $(h_sources)
lib_includedir = $(includedir)/

AM_CFLAGS := -I $(top_srcdir)/inc
AM_CFLAGS += -Wno-unused-parameter
AM_CFLAGS += -DCARD_DEF_FILE=\"/etc/card-defs.xml\"

lib_LTLIBRARIES      = libsndcardparser.la
libsndcardparser_la_SOURCES   = src/snd-card-parser.c
libsndcardparser_la_CFLAGS := $(AM_CFLAGS)
libsndcardparser_la_LDFLAGS   = -lexpat -lpthread
