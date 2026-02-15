AC_PREREQ(2.61)
AC_INIT([libcutils],1.0.0)
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

AC_ARG_WITH([core_includes],
    AC_HELP_STRING([--with-core-includes=@<:@dir@:>@],
       [Specify the location of the core headers]),
    [core_incdir=$withval],
    with_core_includes=no)

if test "x$with_core_includes" != "xno"; then
   CPPFLAGS="${CPPFLAGS} -I${core_incdir}"
   AC_SUBST(core_incdir,${core_incdir})
fi

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

AM_CONDITIONAL(LE_PROPERTIES, [test "x$LE_PROPERTIES_ENABLED" = "xtrue"])

AC_CONFIG_FILES([ \
    Makefile \
    libcutils.pc
])
AC_OUTPUT

