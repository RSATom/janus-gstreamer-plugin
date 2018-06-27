#include "RtspServerMedia.h"

#include <glib.h>

#include "GlibPtr.h"
#include "GstPtr.h"


struct _RtspServerMedia
{
    GstRTSPMedia baseInstance;

    GstElementPtr sinkPtr;
};


G_DEFINE_TYPE(RtspServerMedia, rtsp_server_media, GST_TYPE_RTSP_MEDIA)


GstElement* rtsp_server_media_create_element()
{
    const gchar* pipeline =
        "rtph264depay name=depay0 ! appsink name=sink";

    GError* error = nullptr;
    GstElementPtr elementPtr(
        gst_parse_launch_full(
            pipeline, NULL, GST_PARSE_FLAG_PLACE_IN_BIN,
            &error));
    GErrorPtr errorPtr(error);

    if(errorPtr)
        ;// "Fail to create restream pipeline: {}", errorPtr->message;

    return elementPtr.release();
}

static void
prepared(
    GstRTSPMedia* media,
    gpointer /*userData*/)
{
    RtspServerMedia* self = _RTSP_SERVER_MEDIA(media);

    GstElementPtr pipelinePtr(gst_rtsp_media_get_element(media));
    GstElement* pipeline = pipelinePtr.get();

    GstElementPtr sinkPtr(gst_bin_get_by_name(GST_BIN(pipeline), "sink"));
}

static void
unprepared(
    GstRTSPMedia* media,
    gpointer /*userData*/)
{
}

static void
rtsp_server_media_class_init(RtspServerMediaClass* klass)
{
    // GstRTSPMediaClass* parent_klass = GST_RTSP_MEDIA_CLASS(klass);
}

static void
rtsp_server_media_init(RtspServerMedia* self)
{
    // GstRTSPMedia* parent = GST_RTSP_MEDIA(self);

    g_signal_connect(self, "prepared", G_CALLBACK(prepared), nullptr);
    g_signal_connect(self, "unprepared", G_CALLBACK(unprepared), nullptr);
}

GstElement* rtsp_server_media_get_app_sink(RtspServerMedia* self)
{
    return nullptr;
}
