AC_DEFUN([AC_FLDIGI_FLTK], [
  AC_ARG_VAR([FLTK_CONFIG], [Path to fltk-config utility])
  AC_ARG_VAR([FLTK_CFLAGS], [C compiler flags for FLTK, overriding fltk-config])
  AC_ARG_VAR([FLTK_LIBS], [linker flags for FLTK, overriding fltk-config])

  if test "x$FLTK_CFLAGS" = "x" && test "x$FLTK_LIBS" = "x"; then
      if test "x$FLTK_CONFIG" = "x"; then
          AC_PATH_PROG([FLTK_CONFIG], [fltk-config], [no])
      else
          AC_MSG_CHECKING([for fltk-config])
          AC_MSG_RESULT([$FLTK_CONFIG])
      fi

      if test "$FLTK_CONFIG" = "no"; then
          AC_MSG_ERROR([
  *** The fltk-config script could not be found. Please install the development
  *** headers and libraries for FLTK 1.4 or later, or set PATH to the directory
  *** that contains fltk-config.
          ])
      fi

      FLTK_API_VERSION="`$FLTK_CONFIG --version`"
      if test $? -ne 0; then
          AC_MSG_ERROR([$FLTK_CONFIG failed])
      fi

      AS_VERSION_COMPARE([$FLTK_API_VERSION], [1.4.0],
          [AC_MSG_ERROR([
  *** The version of FLTK found on your system is $FLTK_API_VERSION.
  *** To build $PACKAGE you need FLTK 1.4 or later.
          ])],
          [],
          [])

      FLTK_CFLAGS=`$FLTK_CONFIG --cxxflags`
      if test "x$ac_cv_static" != "xyes"; then
          FLTK_LIBS=`$FLTK_CONFIG --ldflags --use-images`
      else
          FLTK_LIBS=`$FLTK_CONFIG --ldstaticflags --use-images`
      fi
  else
      AC_MSG_NOTICE([not checking for FLTK])
  fi

  AC_SUBST([FLTK_CFLAGS])
  AC_SUBST([FLTK_LIBS])

  if test "x$FLTK_CONFIG" != "x" && test "x$FLTK_CONFIG" != "xno"; then
      AC_DEFINE_UNQUOTED([FLTK_BUILD_VERSION],
          ["`$FLTK_CONFIG --version`"],
          [FLTK version])
  else
      AC_DEFINE_UNQUOTED([FLTK_BUILD_VERSION],
          ["unknown"],
          [FLTK version])
  fi

  AC_ARG_VAR([FLUID], [Fast Light User-Interface Designer])
  AC_CHECK_PROG([FLUID], [fluid], [fluid])
  AM_CONDITIONAL([HAVE_FLUID], [test -n "$FLUID"])
])
