#pragma once

#include <gst/rtsp-server/rtsp-server.h>


G_BEGIN_DECLS

#define TYPE_RTSP_SERVER_MEDIA rtsp_server_media_get_type()
G_DECLARE_FINAL_TYPE(RtspServerMedia, rtsp_server_media, , RTSP_SERVER_MEDIA, GstRTSPMedia)

GstElement* rtsp_server_media_create_element();
GstElement* rtsp_server_media_get_app_sink(RtspServerMedia*);

G_END_DECLS
