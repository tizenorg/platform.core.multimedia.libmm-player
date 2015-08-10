#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([libmm-player],[0.3.0])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])

# Checks for programs.
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_CC

AC_PROG_LIBTOOL

AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_HEADER_STDBOOL
AC_HEADER_STDC
AC_HEADER_TIME
AC_PROG_GCC_TRADITIONAL

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_INT64_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_CHECK_FUNCS([gettimeofday memset strchr strstr])

PKG_CHECK_MODULES(MMCOMMON,mm-common)
AC_SUBST(MMCOMMON_CFLAGS)
AC_SUBST(MMCOMMON_LIBS)

PKG_CHECK_MODULES(MMSOUND,mm-sound)
AC_SUBST(MMSOUND_CFLAGS)
AC_SUBST(MMSOUND_LIBS)

PKG_CHECK_MODULES(GLIB, glib-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GST, gstreamer-1.0 >= 1.2.0)
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

PKG_CHECK_MODULES(GST_PLUGIN_BASE, gstreamer-plugins-base-1.0 >= 1.2.0)
AC_SUBST(GST_PLUGIN_BASE_CFLAGS)
AC_SUBST(GST_PLUGIN_BASE_LIBS)

AC_ARG_ENABLE(wayland, AC_HELP_STRING([--enable-wayland], [enable wayland]),
[
 case "${enableval}" in
         yes) WAYLAND_SUPPORT=yes ;;
         no)  WAYLAND_SUPPORT=no ;;
         *)   AC_MSG_ERROR(bad value ${enableval} for --enable-wayland) ;;
 esac
 ],[WAYLAND_SUPPORT=no])
if test "x$WAYLAND_SUPPORT" = "xyes"; then
PKG_CHECK_MODULES(GST_WAYLAND, gstreamer-wayland-1.0 >= 1.2.0)
AC_SUBST(GST_WAYLAND_CFLAGS)
AC_SUBST(GST_WAYLAND_LIBS)
fi
AM_CONDITIONAL([WAYLAND_SUPPORT], [test "x$WAYLAND_SUPPORT" = "xyes"])

PKG_CHECK_MODULES(GST_VIDEO, gstreamer-video-1.0 >= 1.2.0)
AC_SUBST(GST_VIDEO_CFLAGS)
AC_SUBST(GST_VIDEO_LIBS)

PKG_CHECK_MODULES(GST_APP, gstreamer-app-1.0 >= 1.2.0)
AC_SUBST(GST_APP_CFLAGS)
AC_SUBST(GST_APP_LIBS)

PKG_CHECK_MODULES(APPFWK, appcore-efl)
AC_SUBST(APPFWK_CFLAGS)
AC_SUBST(APPFWK_LIBS)

PKG_CHECK_MODULES(ELEMENTARY, elementary)
AC_SUBST(ELEMENTARY_CFLAGS)
AC_SUBST(ELEMENTARY_LIBS)

# for testsuite
PKG_CHECK_MODULES(MMUTIL, mmutil-imgp)
AC_SUBST(MMUTIL_CFLAGS)
AC_SUBST(MMUTIL_LIBS)

PKG_CHECK_MODULES(EVAS, evas)
AC_SUBST(EVAS_CFLAGS)
AC_SUBST(EVAS_LIBS)

PKG_CHECK_MODULES(ECORE, ecore)
AC_SUBST(ECORE_CFLAGS)
AC_SUBST(ECORE_LIBS)

PKG_CHECK_MODULES(INIPARSER, iniparser)
AC_SUBST(INIPARSER_CFLAGS)
AC_SUBST(INIPARSER_LIBS)

PKG_CHECK_MODULES(VCONF, vconf)
AC_SUBST(VCONF_CFLAGS)
AC_SUBST(VCONF_LIBS)

PKG_CHECK_MODULES(ICU, icu-i18n)
AC_SUBST(ICU_CFLAGS)
AC_SUBST(ICU_LIBS)


PKG_CHECK_MODULES(MEDIAPACKET, capi-media-tool)
AC_SUBST(MEDIAPACKET_CFLAGS)
AC_SUBST(MEDIAPACKET_LIBS)

AC_ARG_ENABLE(sdk, AC_HELP_STRING([--enable-sdk], [sdk build]),
	 [
		case "${enableval}" in
		yes) IS_SDK=yes ;;
		no)  IS_SDK=no ;;
		  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-sdk) ;;
		esac
	],
[IS_SDK=no])
AM_CONDITIONAL([IS_SDK], [test "x$IS_SDK" = "xyes"])

AC_CONFIG_FILES([Makefile
		 src/Makefile
		 mm-player.pc
		 mm-playerclient.pc
])
AC_OUTPUT
