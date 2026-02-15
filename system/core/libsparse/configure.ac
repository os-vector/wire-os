  AC_PREREQ(2.61)

  AC_INIT([libsparse],1.0.0)

  AM_INIT_AUTOMAKE([foreign])

  AM_MAINTAINER_MODE

  AC_CONFIG_HEADER([config.h])
  AC_CONFIG_MACRO_DIR([m4])

  # Checks for programs.
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

  AC_ARG_ENABLE(img-convert-utils,
      AC_HELP_STRING([--disable-img-convert-utils], [Don't build utilities needed for sparse to unsparse image conversion.]),
              [img_convert_utils=${enableval}], [img_convert_utils="no"])

  AM_CONDITIONAL([IMG_CONVERT_UTILS],[test "x${img_convert_utils}" = "xyes"])

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


  AC_OUTPUT([ \
          Makefile \
          libsparse.pc
  ])
