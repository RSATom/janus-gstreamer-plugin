#pragma once

#include <gst/rtsp-server/rtsp-server.h>

class RtspServerProxyMountPoint; // #include "RtspServerProxyMountPoint.h"


G_BEGIN_DECLS

#define TYPE_RTSP_SERVER_MEDIA_FACTORY rtsp_server_media_factory_get_type()
G_DECLARE_FINAL_TYPE(RtspServerMediaFactory, rtsp_server_media_factory, ,RTSP_SERVER_MEDIA_FACTORY, GstRTSPMediaFactory)

RtspServerMediaFactory* rtsp_server_media_factory_new(RtspServerProxyMountPoint*);

G_END_DECLS
