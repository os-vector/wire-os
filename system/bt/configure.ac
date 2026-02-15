#                                               -*- Autoconf -*-

# configure.ac -- Autoconf script for diag
#

# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([fluoride], [1.0.0])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([hci/src/hci_hal.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_PROG_LIBTOOL
# Checks for programs.
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

# Library configs
AC_ARG_WITH([common_includes],
      AC_HELP_STRING([--with-common-includes=@<:@dir@:>@],
         [Specify the location of the common headers]),
      [common_incdir=$withval],
      with_common_includes=no)

if test "x$with_common_includes" != "xno"; then
   CFLAGS="${CFLAGS} -I${common_incdir}"
fi

AC_ARG_WITH([hardware_include],
                  AC_HELP_STRING([--with-hardware-include=@<:@dir@:>@],
                                 [Specify the location of the hardware headers]),
                                 [hardware_incdir=$withval],
                                 with_hardware_include=no)

if test "x$with_hardware_include" != "xno"; then
       CPPFLAGS="${CPPFLAGS} -I${hardware_incdir}"
fi

AC_ARG_WITH([hardware_libraries],
                 AC_HELP_STRING([--with-hardware-libraries=@<:@dir@:>@],
                                     [Specify the location of the hardware libraries]),
                                     [hardware_libdir=$withval],
                                          with_hardware_libraries=no)

if test "x$with_hardware_libraries" != "xno"; then
    LDFLAGS="${LDFLAGS} -L${hardware_libdir}"
fi

AC_SUBST([CFLAGS])
AC_SUBST([CPPFLAGS])
AC_SUBST([CC])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/socket.h sys/time.h termios.h unistd.h])

# Build all components by default
#
# To add new components: add AC_ARG_ENABLE, AM_CONDITIONAL and update AC_OUTPUT
#
AC_ARG_ENABLE(target,
  [AS_HELP_STRING([--enable-target=TARGET], [Specify the target product to build])],
  [TARGET=$enableval],
  [TARGET=none]
)

AC_ARG_ENABLE(rome,
  [AS_HELP_STRING([--enable-rome=ROME], [Specify the rome product to build])],
  [ROME=$enableval],
  [ROME=none]
)

AM_CONDITIONAL([MDM_ROME], [test "x$TARGET" = "xmdm9607" -o "x$TARGET" = "xmdm9635" -o "x$TARGET" = "xmdm9640" -o "x$TARGET" = "xmdm9650" -o "x$TARGET" = "xapq8096" -o "x$TARGET" = "xsdx20" -o "x$ROME" = "xqsap" -o "x$ROME" = "xrobot-rome"])
AM_CONDITIONAL([MDM_PRONTO], [test "x$TARGET" = "xapq8009" -o "x$TARGET" = "xapq8017" -o "x$TARGET" = "xapq8053" -o "x$TARGET" = "xapq8016"])
AM_CONDITIONAL([MDM_CHEROKEE], [test "x$TARGET" = "xapq8098"])
AM_CONDITIONAL([BT_AUDIO_HAL_INTEGRATION], [test "x$TARGET" = "xapq8009" -o "x$TARGET" = "xapq8017" -o "x$TARGET" = "xapq8053" -o "x$TARGET" = "xapq8098"])
# BTC support
# make below flag to true to enable the BTC
AM_CONDITIONAL([BTC_SUPPORT], false)

# SSR_Cleanup support
# make below flag to true to cleanup audio related things from BT app
# on detection of SSR/command timeout/Hw eror event
AM_CONDITIONAL([SSR_CLEANUP_SUPPORT], true)


AC_MSG_CHECKING(for zlib support)
AC_ARG_WITH(zlib, AC_HELP_STRING([--with-zlib],[Enable zlib support for mod_compress]),
    [WITH_ZLIB=$withval],[WITH_ZLIB=yes])
AC_MSG_RESULT([$WITH_ZLIB])

if test "$WITH_ZLIB" != "no"; then
  AC_CHECK_LIB(z, deflate, [
    AC_CHECK_HEADERS([zlib.h],[
      Z_LIB=-lz
      AC_DEFINE([HAVE_LIBZ], [1], [libz])
      AC_DEFINE([HAVE_ZLIB_H], [1])
    ])
  ])
  if test x$Z_LIB = x; then
     AC_MSG_ERROR([zlib-headers and/or libs where not found, install them or build with --without-zlib])
  fi
fi
AC_SUBST(Z_LIB)

AC_ARG_WITH([lib_path],
      AC_HELP_STRING([--with-lib-path=@<:@dir@:>@],
         [Specify the location of the libraries]),
      [std_libdir=$withval],
      with_lib_path=no)

if test "x$with_lib_path" != "xno"; then
   SYS_LIB=${std_libdir}
fi

AC_SUBST(SYS_LIB)

AC_CHECK_FUNC(signalfd, dummy=yes,
      AC_MSG_ERROR(signalfd support is required))

AC_CHECK_LIB(rt, clock_gettime, dummy=yes,
      AC_MSG_ERROR(realtime clock support is required))

AC_CHECK_LIB(pthread, pthread_create, dummy=yes,
      AC_MSG_ERROR(posix thread support is required))

AC_CHECK_LIB(dl, dlopen, dummy=yes,
      AC_MSG_ERROR(dynamic linking loader is required))

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday memset select socket strerror])

AC_CONFIG_FILES([ \
         Makefile \
         hci/Makefile \
         osi/Makefile \
         profile/Makefile \
         device/Makefile \
         utils/Makefile \
         btcore/Makefile \
         stack/Makefile \
         bta/Makefile \
         main/Makefile \
         tools/Makefile \
         tools/bdtool/Makefile \
         tools/btsnoop_dump/Makefile \
         embdrv/sbc/decoder/Makefile \
         btif/Makefile
         audio_a2dp_hw/Makefile \
         test/Makefile \
         test/bluedroidtest/Makefile \
         ])

AC_OUTPUT
