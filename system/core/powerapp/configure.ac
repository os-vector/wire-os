  AC_PREREQ(2.61)
  
  AC_INIT([system-core-powerapp],1.0.0)

  AC_CONFIG_AUX_DIR([build-aux])  

  AM_INIT_AUTOMAKE([-Wall gnu foreign])
  
  AM_MAINTAINER_MODE
  
  AC_CONFIG_HEADER([config.h])

  # Checks for programs.
  AM_PROG_AS
  AC_PROG_CC
  AM_PROG_CC_C_O
  AC_PROG_LIBTOOL
  AC_PROG_AWK
  AC_PROG_CPP
  AC_PROG_INSTALL
  AC_PROG_LN_S
  AC_PROG_MAKE_SET
 
  #AC_CHECK_HEADERS([sys/uio.h])
  
  #
  # Build all components by default
  #
  # To add new components: add AC_ARG_ENABLE, AM_CONDITIONAL and update AC_OUTPUT
  #
  
 
  AS_CASE([$host],
          [arm*], [ARM=yes],
          [ARM=no]
  )
  AM_CONDITIONAL(ARM, [test "x$ARM" = "xyes"])
  
  AC_CHECK_FUNCS([strlcpy],[STRLCPY=yes],[STRLCPY=no])
  AM_CONDITIONAL([HAVE_STRLCPY],[test "x$STRLCPY" = "xyes"])
  
  AC_OUTPUT([ \
          Makefile 
  ])
