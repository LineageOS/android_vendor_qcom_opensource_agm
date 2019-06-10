pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = agmserver.pc
EXTRA_DIST = $(pkgconfig_DATA)

h_sources  = $(top_srcdir)/inc/ipc_interface.h
h_sources += $(top_srcdir)/inc/agm_death_notifier.h

AM_CPPFLAGS := -I $(top_srcdir) -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/agm/\
               -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/qti-agm-service\
               -I ${top_srcdir}/inc

AM_CPPFLAGS += -D__unused=__attribute__\(\(__unused__\)\)

library_include_HEADERS = $(h_sources)
library_includedir = $(includedir)/qti-agm-service/

lib_LTLIBRARIES = libagmserverwrapper.la
libagmserverwrapper_la_SOURCES   = ${top_srcdir}/src/agm_death_notifier.cpp ${top_srcdir}/src/agm_server_wrapper.cpp

libagmserverwrapper_la_CPPFLAGS := $(AM_CPPFLAGS)
libagmserverwrapper_la_LIBADD = -lagm
libagmserverwrapper_la_LDFLAGS   = -lcutils -llog -ldl -lbinder -shared -avoid-version -lutils

lib_LTLIBRARIES += libagmproxy.la
libagmproxy_la_SOURCES   = ${top_srcdir}/src/ipc_proxy_server.cpp 
libagmproxy_la_CPPFLAGS := $(AM_CPPFLAGS)
libagmproxy_la_LIBADD = libagmserverwrapper.la
libagmproxy_la_LDFLAGS   = -lcutils -llog -ldl -lbinder -shared -avoid-version -lutils

bin_PROGRAMS := agm_server
agm_server_SOURCES  := ${top_srcdir}/src/agm_server_daemon.cpp

agmserver_la_CPPFLAGS := $(AM_CPPFLAGS)
agm_server_LDADD    := libagmproxy.la
agmserver_la_LDFLAGS   = -lcutils -llog -ldl -lbinder -shared -avoid-version -lutils
