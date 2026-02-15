AC_PREREQ(2.61)
AC_INIT([libbase],1.0.0)
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


#obtain  system-core source path.

AC_ARG_WITH([core_sourcedir],
    AC_HELP_STRING([--with-core-sourcedir=@<:@dir@:>@],
       [Specify the location of the core headers]),
    [core_source=$withval],
    with_core_sourcedir=no)

if test "x$with_core_sourcedir" != "xno"; then
   AC_SUBST(core_srcdir,${core_source})
fi

#check if the build is for host or target

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

#output the configured files

AC_CONFIG_FILES([ \
    Makefile \
    libbase.pc
])
AC_OUTPUT

