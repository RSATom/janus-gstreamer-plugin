AC_INIT([janus-gstreamer], [0.1], [RSATom@gmail.com])
AC_CONFIG_MACRO_DIR([m4])

AC_ENABLE_SHARED(yes)
AC_DISABLE_STATIC(yes)

AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_PROG_CXX

LT_INIT

PKG_CHECK_MODULES([GSTREAMER], [gstreamer-1.0])
PKG_CHECK_MODULES([GSTREAMER_SDP], [gstreamer-sdp-1.0])
PKG_CHECK_MODULES([GSTREAMER_APP], [gstreamer-app-1.0])
PKG_CHECK_MODULES([GSTREAMER_RTSP_SERVER], [gstreamer-rtsp-server-1.0])

AC_CONFIG_FILES([Makefile plugins/gstreamer/Makefile])
AM_SILENT_RULES([yes])
AC_OUTPUT
