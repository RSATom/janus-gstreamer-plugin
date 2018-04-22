JANUS_PATH=$(exec_prefix)

AM_CXXFLAGS = -I$(JANUS_PATH)/include -I$(JANUS_PATH)/include/janus

lib_LTLIBRARIES = libjanus_gstreamer.la
libjanus_gstreamer_la_SOURCES = \
    QueueSource.cpp \
    Session.cpp \
    Media.cpp \
    MountPoint.cpp \
    janus_gstreamer.cpp
libjanus_gstreamer_la_CXXFLAGS = $(AM_CXXFLAGS) -std=c++14 -Wreturn-type $(GSTREAMER_CFLAGS) $(GSTREAMER_SDP_CFLAGS) $(GSTREAMER_APP_CFLAGS)
libjanus_gstreamer_la_LDFLAGS = $(GSTREAMER_LIBS) $(GSTREAMER_SDP_LIBS) $(GSTREAMER_APP_LIBS) -L$(JANUS_PATH)/lib
libdir = $(exec_prefix)/lib/janus/plugins
