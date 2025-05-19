#                                               -*- Autoconf -*-
# configure.ac -- Autoconf script for spf
#

# Process this file with autoconf to produce a configure script

# Requires autoconf tool later than 2.61
AC_PREREQ(2.69)

AC_INIT([agm],1.0.1)
# Does not strictly follow GNU Coding standards
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
LT_INIT
AC_PROG_CC
AC_PROG_LIBTOOL
AM_PROG_AR
AC_PROG_CXX

AC_ARG_WITH([syslog],
    AS_HELP_STRING([use syslog (default is no)]),
    [with_syslog=$withval],
    [with_syslog=no])
AM_CONDITIONAL([USE_SYSLOG], [test "x${with_syslog}" = "xyes"])

AC_ARG_WITH([glib],
      AC_HELP_STRING([--with-glib],
         [enable glib, Build against glib. Use this when building for HLOS systems which use glib]))

if (test "x${with_glib}" = "xyes"); then
        PKG_CHECK_MODULES(GTHREAD, gthread-2.0 >= 2.16, dummy=yes,
                                AC_MSG_ERROR(GThread >= 2.16 is required))
        PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.16, dummy=yes,
                                AC_MSG_ERROR(GLib >= 2.16 is required))
        GLIB_CFLAGS="$GLIB_CFLAGS $GTHREAD_CFLAGS"
        GLIB_LIBS="$GLIB_LIBS $GTHREAD_LIBS"

        AC_SUBST(GLIB_CFLAGS)
        AC_SUBST(GLIB_LIBS)
fi

AM_CONDITIONAL(USE_GLIB, test "x${with_glib}" = "xyes")

AC_ARG_WITH([agm_no_ipc],
    AS_HELP_STRING([enable ipc communication to AGM (default is no)]),
    [with_agm_no_ipc=$withval],
    [with_agm_no_ipc=no])
AM_CONDITIONAL([AGM_NO_IPC], [test "x${with_agm_no_ipc}" = "xyes"])


PKG_CHECK_MODULES([SPF], [spf])
AC_SUBST(SPF_CFLAGS)


PKG_CHECK_MODULES([MMHEADERS], [mm-audio-headers])
AC_SUBST([MMHEADERS_CFLAGS])

#TODO: Check dependency only if --with-ipc is set
#PKG_CHECK_MODULES(DBUS, [ dbus-1 >= 1.4.12 ], dummy=yes,
#                 AC_MSG_ERROR(dbus-1 >= 1.4.12 is required))

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

AC_CONFIG_FILES(
Makefile
service/Makefile
service/agm.pc
ipc/DBus/agm_client/Makefile
ipc/DBus/agm_client/agmclient.pc
ipc/DBus/agm_server/Makefile
ipc/DBus/agm_server/agmserver.pc
snd_parser/Makefile
snd_parser/sndparser.pc
plugins/tinyalsa/Makefile
plugins/tinyalsa/agmplugin.pc
plugins/tinyalsa/test/Makefile
plugins/tinyalsa/test/agmtest.pc
)

AC_OUTPUT
