AC_PREREQ(2.61)
AC_INIT([system-core],1.0.0)
AM_INIT_AUTOMAKE([-Wall gnu foreign])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Check for programs
AM_PROG_AS
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG

# Checks for libraries.
PKG_CHECK_MODULES([LIBSELINUX], [libselinux])
AC_SUBST([LIBSELINUX_CFLAGS])
AC_SUBST([LIBSELINUX_LIBS])

PKG_CHECK_MODULES([LIBBASE], [libbase])

AS_CASE([$host],
        [arm*], [ARM=yes],
        [ARM=no]
)
AM_CONDITIONAL(ARM, [test "x$ARM" = "xyes"])

AS_CASE([$host],
        [aarch64*], [ARM64=yes],
        [ARM64=no]
)
AM_CONDITIONAL(ARM64, [test "x$ARM64" = "xyes"])

AS_CASE([$host],
        [x86_64*], [X86_64=yes],
        [X86_64=no]
)
AM_CONDITIONAL(X86_64, [test "x$X86_64" = "xyes"])

AM_CONDITIONAL(BUILD_HOST, [test "x$X86_64" = "xyes"])
AM_CONDITIONAL(BUILD_DEVICE, [test "x$ARM$ARM64" != "xnono"])

AC_ARG_WITH([host-os],
        [AS_HELP_STRING([--with-host-os=HOST_OS],[Host OS to build adb host.(default: linux)])],
        [HOST_OS="$withval"],
        [HOST_OS='linux'])

if test "x$HOST_OS" = "xlinux-gnueabi"; then
  HOST_OS=linux
else
  AC_MSG_NOTICE([Unknown host OS $HOST_OS detected.])
fi

AM_CONDITIONAL([HOST_OS_LINUX], [test "x$HOST_OS" = "xlinux"])

AC_ARG_WITH(sanitized-headers,
      AS_HELP_STRING([--with-sanitized-headers=DIR],
         [Specify the location of the sanitized Linux headers]),
      [CPPFLAGS="$CPPFLAGS -idirafter $withval"])

AC_ARG_WITH([glib],
      AC_HELP_STRING([--with-glib],
         [enable glib, building HLOS systems which use glib]))

if (test "x${with_glib}" = "xyes"); then
        AC_DEFINE(ENABLE_USEGLIB, 1, [Define if HLOS systems uses glib])
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

AC_ARG_WITH([logd-logging],
      AC_HELP_STRING([--with-logd-logging],
         [enable logd based logging]))

AM_CONDITIONAL(USE_LOGD, [test "x${with_logd_logging}" = "xyes"])

AC_ARG_ENABLE(libsync,
      AC_HELP_STRING([--disable-libsync], [Disable libsync for target]),
         [libsync="no"], [libsync="yes"])

AM_CONDITIONAL(NO_LIBSYNC, [test "x${libsync}" = "xno"])

AC_ARG_ENABLE(adb-root,
      AC_HELP_STRING([--disable-adb-root], [Disable root privileges for adbd]),
         [adb_root=${enableval}], [adb_root="yes"])

AM_CONDITIONAL(ADB_ROOT, [test "x${adb_root}" = "xyes"])

AC_ARG_ENABLE(debuggerd,
      AC_HELP_STRING([--disable-debuggerd], [Disable debuggerd for target]),
         [debuggerd="no"], [debuggerd="yes"])

AM_CONDITIONAL(NO_DEBUGGERD, [test "x${debuggerd}" = "xno"])

AC_ARG_ENABLE(logd-privs,
      AC_HELP_STRING([--enable-logd-privs], [Drop logd priviledges]),
         [logd_privs="yes"], [logd_privs="no"])

AM_CONDITIONAL(LOGDPRIVS, [test "x${logd_privs}" = "xyes"])

AC_ARG_WITH([system-size],
        [AS_HELP_STRING([--with-system-size=SYSTEM_SIZE],[Size of system partition.])],
        [SYSTEM_SIZE="$withval"],
        [SYSTEM_SIZE="0"])

AC_SUBST(SYSTEM_SIZE)

AC_MSG_NOTICE([System of size $SYSTEM_SIZE detected.])

AC_ARG_WITH([fec-size],
        [AS_HELP_STRING([--with-fec-size=FEC_SIZE],[Size of fec image .])],
        [FEC_SIZE="$withval"],
        [FEC_SIZE="0"])

AC_SUBST(FEC_SIZE)

AC_MSG_NOTICE([fec image size $FEC_SIZE detected.])

AC_CONFIG_FILES([ \
    Makefile \
    logwrapper/Makefile \
    logwrapper/logwrapper.pc \
    libsync/Makefile \
    libsync/libsync.pc \
    fs_mgr/Makefile \
    fs_mgr/fs_mgr.pc \
    libsysutils/Makefile \
    logcat/Makefile \
    logd/Makefile \
    adb/Makefile \
    fastboot/Makefile \
    leproperties/Makefile \
    toolbox/Makefile \
    usb/Makefile \
    include/Makefile \
    libbacktrace/Makefile \
    libbacktrace/libbacktrace.pc \
    debuggerd/Makefile
])
AC_OUTPUT
