#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_COPYRIGHT([Copyright (C) 2007, 2008, 2009 Stelios Bounanos, M0GLD (m0gld AT enotty DOT net)])

AC_PREREQ([2.71])

dnl VERSION is the authoritative fldigi version.
m4_define([FLDIGI_VERSION_STRING], m4_esyscmd_s([cat VERSION]))

AC_INIT([fldigi], [FLDIGI_VERSION_STRING],
        [fldigi-devel@sourceforge.net])

dnl Derive the individual version components.
dnl FLDIGI_VERSION_PATCH retains the historical leading dot, e.g. ".13".
FLDIGI_VERSION_MAJOR=`echo "$PACKAGE_VERSION" | sed 's/\..*//'`
FLDIGI_VERSION_MINOR=`echo "$PACKAGE_VERSION" | sed 's/^[^.]*\.//;s/\..*//'`
FLDIGI_VERSION_PATCH=`echo "$PACKAGE_VERSION" | sed 's/^[^.]*\.[^.]*//'`
FLDIGI_VERSION="$PACKAGE_VERSION"

# substitute in Makefiles
AC_SUBST([FLDIGI_VERSION_MAJOR])
AC_SUBST([FLDIGI_VERSION_MINOR])
AC_SUBST([FLDIGI_VERSION_PATCH])
AC_SUBST([FLDIGI_VERSION])

# define in config.h
AC_DEFINE_UNQUOTED([FLDIGI_VERSION_MAJOR], [$FLDIGI_VERSION_MAJOR],
                   [Fldigi major version number])
AC_DEFINE_UNQUOTED([FLDIGI_VERSION_MINOR], [$FLDIGI_VERSION_MINOR],
                   [Fldigi minor version number])
AC_DEFINE_UNQUOTED([FLDIGI_VERSION_PATCH], ["$FLDIGI_VERSION_PATCH"],
                   [Fldigi patch/alpha version string])
AC_DEFINE_UNQUOTED([FLDIGI_VERSION], ["$FLDIGI_VERSION"],
                   [Fldigi version string])

AC_SUBST([AC_CONFIG_ARGS], [$ac_configure_args])

AC_CONFIG_AUX_DIR([build-aux])

# define build, build_cpu, build_vendor, build_os
AC_CANONICAL_BUILD
# define host, host_cpu, host_vendor, host_os
AC_CANONICAL_HOST
# define target, target_cpu, target_vendor, target_os
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([-Wall -Wno-portability foreign std-options subdir-objects 1.9.6])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/main.cxx])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIR([m4])


FLDIGI_AUTHORS="Dave Freese, Stelios Bounanos, and others"
PACKAGE_HOME="https://www.w1hkj.org/Fldigi.html"
PACKAGE_DL="https://www.w1hkj.org/download.html"
PACKAGE_PROJ="http://sourceforge.net/projects/fldigi/files/"
PACKAGE_NEWBUG=
PACKAGE_DOCS="https://www.w1hkj.org/FldigiHelp/index.html"
PACKAGE_GUIDE="https://www.w1hkj.org/beginners.html"
AC_DEFINE_UNQUOTED([FLDIGI_AUTHORS], ["$FLDIGI_AUTHORS"], [Fldigi authors])
AC_DEFINE_UNQUOTED([PACKAGE_HOME], ["$PACKAGE_HOME"], [Home page])
AC_DEFINE_UNQUOTED([PACKAGE_DL], ["$PACKAGE_DL"], [Download page])
AC_DEFINE_UNQUOTED([PACKAGE_PROJ], ["$PACKAGE_PROJ"], [Sourceforge page])
AC_DEFINE_UNQUOTED([PACKAGE_NEWBUG], ["$PACKAGE_NEWBUG"], [Trac new ticket page])
AC_DEFINE_UNQUOTED([PACKAGE_DOCS], ["$PACKAGE_DOCS"], [Docs index])
AC_DEFINE_UNQUOTED([PACKAGE_GUIDE], ["$PACKAGE_GUIDE"], [Beginners guide])
AC_SUBST([FLDIGI_AUTHORS], [$FLDIGI_AUTHORS])
AC_SUBST([PACKAGE_HOME], [$PACKAGE_HOME])
AC_SUBST([PACKAGE_DL], [$PACKAGE_DL])
AC_SUBST([PACKAGE_PROJ], [$PACKAGE_PROJ])
AC_SUBST([PACKAGE_NEWBUG], [$PACKAGE_NEWBUG])
AC_SUBST([PACKAGE_DOCS], [$PACKAGE_DOCS])
AC_SUBST([PACKAGE_GUIDE], [$PACKAGE_GUIDE])


# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS

# Require C++11 support
AX_CXX_COMPILE_STDCXX_11(, mandatory)

# Checks for header files.
AC_CHECK_INCLUDES_DEFAULT
AC_PROG_EGREP
AC_HEADER_DIRENT
AC_CHECK_HEADERS([arpa/inet.h execinfo.h fcntl.h limits.h memory.h netdb.h netinet/in.h regex.h stdint.h stdlib.h string.h strings.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h sys/utsname.h termios.h unistd.h values.h linux/ppdev.h dev/ppbus/ppi.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_ERROR_AT_LINE
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STRFTIME
AC_FUNC_STRTOD
AC_CHECK_FUNCS([getaddrinfo gethostbyname hstrerror gmtime_r localtime_r memmove memset mkdir select setenv snprintf socket socketpair strcasecmp strcasestr strchr strdup strerror strlcpy strncpy strncmp strnlen strncasecmp strrchr strstr strtol uname unsetenv vsnprintf])

# Check for O_CLOEXEC
AC_FCNTL_FLAGS

AC_PRESERVE_HELP_ORDER

### Internationalization
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.21])
AM_CONDITIONAL([USE_NLS], [test "x$USE_NLS" = "xyes"])

# Determine whether fldigi has been requested.
# Set WANT_FLDIGI Makefile conditional.
AC_FLDIGI_PROGRAMS

###### OS support
### OSX
# Set ac_cv_mac_universal to yes/no
# Set DARWIN Makefile conditional
# Substitute MAC_UNIVERSAL_CFLAGS and MAC_UNIVERSAL_LDFLAGS in Makefile
AC_FLDIGI_MACOSX

### win32
# Set WIN32 Makefile conditional
# Set HAVE_WINDRES Makefile conditional
# Substitute WINDRES in Makefile
AC_FLDIGI_WIN32

### Non-POSIX compatibility (i.e. mingw32)
# Sets various Makefile conditionals; see m4/np-compat.m4
AC_FLDIGI_NP_COMPAT


### static flag
# Set ac_cv_static to yes/no
# Substitute RTLIB in Makefile
AC_FLDIGI_STATIC

### optimizations
# Set ac_cv_opt to arg
# Substitute OPT_FLAGS in Makefile
AC_FLDIGI_OPT

### debug flag
# Set ac_cv_debug to yes/no
# Override CXXFLAGS
# Set ENABLE_DEBUG Makefile conditional
# Substitute RDYNAMIC in Makefile
AC_FLDIGI_DEBUG

### benchmark mode
# Set ac_cv_benchmark to yes/no
# Define BENCHMARK_MODE in config.h
# Set ENABLE_BENCHMARK Makefile conditional
AC_FLDIGI_BENCHMARK

### TLS flag
# Set ac_cv_tls to yes/no
# Define USE_TLS in config.h
AC_FLDIGI_TLS

### FLTK
# Substitute FLTK_CFLAGS and FLTK_LIBS in Makefile
# Set FLUID variable
# Set HAVE_FLUID Makefile conditional
AC_FLDIGI_FLTK

### OSS
# Set ac_cv_oss to yes/no
# Define USE_OSS in config.h
AC_FLDIGI_OSS

### libpng
# Required if $ac_cv_want_fldigi is "yes"
# Set ac_cv_png to yes/no    (not used)
# Define USE_PNG in config.h (as above)
# Substitute PNG_CFLAGS and PNG_LIBS in Makefile
AC_FLDIGI_PKG_CHECK([png], [libpng >= 1.2.8], [no], [no])
PNG_LIBS=`$PKG_CONFIG --static --libs "libpng >= 1.2.8"`

### libgpiod
# Set ac_cv_libgpiod to yes/no
# Define USE_LIBGPIOD in config.h
# Substitute LIBGPIOD_CFLAGS and LIBGPIOD_LIBS in Makefile
if test "x$ac_cv_want_fldigi" = "xyes"; then
    AC_FLDIGI_PKG_CHECK([libgpiod], [libgpiod >= 2.2.1], [no], [yes])
fi

### libsamplerate
# Required if $ac_cv_want_fldigi is "yes"
# Set ac_cv_samplerate to yes/no    (not used)
# Define USE_SAMPLERATE in config.h (as above)
# Substitute SAMPLERATE_CFLAGS and SAMPLERATE_LIBS in Makefile
if test "x$ac_cv_want_fldigi" = "xyes"; then
    AC_FLDIGI_PKG_CHECK([samplerate], [samplerate >= 0.1.1], [no], [no])
fi

### libsndfile
# Set ac_cv_sndfile to yes/no
# Define USE_SNDFILE in config.h
# Substitute SNDFILE_CFLAGS and SNDFILE_LIBS in Makefile
# name, version, show-in-help?, optional?, help-text, [am-cond]
if test "x$ac_cv_want_fldigi" = "xyes"; then
    AC_FLDIGI_PKG_CHECK([sndfile], [sndfile >= 1.0.10], [yes], [no],
                    [enable reading/writing of audio files via libsndfile @<:@autodetect@:>@] )
fi

### portaudio
# Set ac_cv_portaudio to yes/no
# Define USE_PORTAUDIO in config.h
# Substitute PORTAUDIO_CFLAGS and PORTAUDIO_LIBS in Makefile
if test "x$ac_cv_want_fldigi" = "xyes"; then
    AC_FLDIGI_PKG_CHECK([portaudio], [portaudio-2.0 >= 19], [no], [no])
fi

### pulseaudio
# Set ac_cv_pulseaudio to yes/no
# Define USE_PULSEAUDIO in config.h
# Substitute PULSEAUDIO_CFLAGS and PULSEAUDIO_LIBS in Makefile
AC_FLDIGI_PKG_CHECK([pulseaudio], [libpulse-simple >= 0.9.7], [yes], [yes],
                    [enable support for PulseAudio @<:@autodetect@:>@] )

if test "x$ac_cv_oss" = "xno" && \
   test "x$ac_cv_portaudio" = "xno" && \
   test "x$ac_cv_pulseaudio" = "xno"; then
    AC_MSG_WARN([*** $PACKAGE will be compiled without audio device support ***])
fi

### hamlib
# Set ac_cv_hamlib to yes/no
# Define USE_HAMLIB in config.h
# Substitute HAMLIB_CFLAGS and HAMLIB_LIBS in Makefile
# Set ENABLE_HAMLIB Makefile conditional
AC_FLDIGI_PKG_CHECK([hamlib], [hamlib >= 1.2.4], [yes], [yes],
                    [use hamradio control libraries @<:@autodetect@:>@],
                    [ENABLE_HAMLIB])
# We compile our own copy of hamlib's locator.c if hamlib is not available
# or if it is version <= 1.2.9, which has a broken azimuth_long_path function
AX_COMPARE_VERSION([$pkg_hamlib_version], [le], [1.2.9], [need_locator_c=yes], [need_locator_c=no])
AM_CONDITIONAL([NEED_HAMLIB_LOCATOR], [test "$need_locator_c" = "yes"])

### X11
# Set ac_cv_x to yes/no
# Define USE_X in config.h
# Substitute X_CFLAGS and X_LIBS in Makefile
if test "x$target_darwin" = "xno" && test "x$target_win32" = "xno"; then
    AC_FLDIGI_PKG_CHECK([x], [x11], [no], [yes])
fi

### libflxmlrpc
# Set ac_cv_flxmlrpc to yes/no
# Define USE_FLXMLRPC in config.h
# Substitute FLXMLRPC_CFLAGS and FLXMLRPC_LIBS in Makefile

AC_FLDIGI_PKG_CHECK([flxmlrpc], [flxmlrpc >= 1.0.1],
  [yes], [yes], [use external flxmlrpc library @<:@autodetect@:>@],
  [ENABLE_FLXMLRPC])

### libmbedtls
# Set ac_cv_mbedtls to yes/no
# Define USE_LIBMBEDTLS in config.h
# Substitute LIBMBEDTLS_CFLAGS and LIBMBEDTLS_LIBS in Makefile
AC_FLDIGI_PKG_CHECK([libmbedtls], [libmbedtls >= 2.16.0], 
  [yes], [yes], [use mbedtls libraries @<:@autodetect@:>@],
  [ENABLE_LIBMBEDTLS])

### libintl
# Substitute INTL_CFLAGS in Makefile
# Substitute INTL_LIBS in Makefile
# (default to LIBINTL)
AC_FLDIGI_INTL

### libhid
# Set ac_cv_udev to yes/no
# Define USE_LIBHID in config.h
# Substitute LIBHID_CFLAGS and LIBHID_LIBS in Makefile
#if test "x$ac_cv_want_fldigi" = "xyes"; then
#AC_FLDIGI_PKG_CHECK([libhid], [libhid], [no], [no],
#  [use udev libraries @<:@autodetect@:>@],
#  [ENABLE_LIBHID])
#fi

### asciidoc
# substitute ASCIIDOC and A2X in doc/Makefile
# set HAVE_ASCIIDOC Makefile conditional
AC_FLDIGI_DOCS

### build info
# Define various build variables in config.h
AC_FLDIGI_BUILD_INFO

### silent build rules
AC_FLDIGI_BUILD_RULES

### output
AH_TOP([
#ifndef CONFIG_H_
#define CONFIG_H_
])
AH_BOTTOM([
#include "pkg.h"
#include "util.h"
#endif /* CONFIG_H_ */
])
AC_CONFIG_FILES([Makefile src/Makefile po/Makefile.in doc/Makefile])
AC_OUTPUT


### summary
AC_MSG_RESULT([
Configuration summary:

  Version ..................... $VERSION

  Static linking .............. $ac_cv_static
  CPU optimizations ........... $ac_cv_opt
  Debugging ................... $ac_cv_debug

  fldigi ...................... $ac_cv_want_fldigi

  i18n ........................ $USE_NLS
])

ac_xmlrpc_report="internal lib";

if test "x$ac_cv_flxmlrpc" = "xyes"; then
  ac_xmlrpc_report="external lib";
fi

ac_mbedtls_report="internal lib";

if test "x$ac_cv_libmbedtls" = "xyes"; then
  ac_mbedtls_report="external lib";
fi

if test "x$ac_cv_want_fldigi" = "xyes"; then
  AC_MSG_RESULT([ fldigi build options:

  oss ......................... $ac_cv_oss
  portaudio ................... $ac_cv_portaudio
  pulseaudio .................. $ac_cv_pulseaudio
  libgpiod .................... $ac_cv_libgpiod

  libmbedtls .................. $ac_mbedtls_report
  flxmlrpc .................... $ac_xmlrpc_report

  hamlib ...................... $ac_cv_hamlib
])
fi
