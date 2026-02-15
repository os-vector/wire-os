AC_PREREQ(2.61)
AC_INIT([libutils],1.0.0)
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

AC_ARG_WITH([system-core-includes],
    AC_HELP_STRING([--with-system-core-includes=@<:@dir@:>@],
       [Specify the location of the system core headers]),
    [CPPFLAGS="$CPPFLAGS -idirafter $withval"],
    with_system_core_includes=no)

if test "x$with_system_core_includes" != "xno"; then
   AC_SUBST(SYSTEMCORE_INCLUDES,${withval})
fi

AC_ARG_WITH([liblog-includes],
    AC_HELP_STRING([--with-liblog-includes=@<:@dir@:>@],
       [Specify the location of the liblog headers]),
    [CPPFLAGS="$CPPFLAGS -idirafter $withval"],
    with_liblog_includes=no)

AC_CONFIG_FILES([ \
    Makefile \
    libutils.pc
])
AC_OUTPUT
