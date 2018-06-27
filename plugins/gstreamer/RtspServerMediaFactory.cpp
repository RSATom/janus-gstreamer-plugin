#include "RtspServerMediaFactory.h"

#include "RtspServerMedia.h"

#include "RtspServerProxyMountPoint.h"


namespace
{

struct CxxPrivate
{
    RtspServerProxyMountPoint* proxyMountPoint;
};

}

struct _RtspServerMediaFactory
{
    GstRTSPMediaFactory baseInstance;

    CxxPrivate* p;
};

static GstElement* create_element(GstRTSPMediaFactory* factory, const GstRTSPUrl* url);


G_DEFINE_TYPE(RtspServerMediaFactory, rtsp_server_media_factory, GST_TYPE_RTSP_MEDIA_FACTORY)


RtspServerMediaFactory* rtsp_server_media_factory_new(RtspServerProxyMountPoint*)
{
    RtspServerMediaFactory* instance =
        _RTSP_SERVER_MEDIA_FACTORY(g_object_new(TYPE_RTSP_SERVER_MEDIA_FACTORY, NULL));

    return instance;
}

static void rtsp_server_media_factory_class_init(RtspServerMediaFactoryClass* klass)
{
    GstRTSPMediaFactoryClass* parent_klass = GST_RTSP_MEDIA_FACTORY_CLASS(klass);

    parent_klass->create_element = create_element;
}

static void media_configure(
    GstRTSPMediaFactory* factory,
    GstRTSPMedia* media,
    gpointer)
{
}

static void media_constructed(
    GstRTSPMediaFactory* factory,
    GstRTSPMedia* media,
    gpointer)
{
}

static void rtsp_server_media_factory_init(RtspServerMediaFactory* self)
{
    self->p = new CxxPrivate;

    GstRTSPMediaFactory* parent = GST_RTSP_MEDIA_FACTORY(self);

    gst_rtsp_media_factory_set_transport_mode(
        parent, GST_RTSP_TRANSPORT_MODE_RECORD);
    gst_rtsp_media_factory_set_shared(parent, TRUE);

    gst_rtsp_media_factory_set_media_gtype(parent, TYPE_RTSP_SERVER_MEDIA);

    g_signal_connect(self, "media-configure", G_CALLBACK(media_configure), nullptr);
    g_signal_connect(self, "media-constructed", G_CALLBACK(media_constructed), nullptr);
}

static GstElement* create_element(GstRTSPMediaFactory* factory, const GstRTSPUrl* url)
{
    RtspServerMediaFactory* self = _RTSP_SERVER_MEDIA_FACTORY(factory);

    return rtsp_server_media_create_element();
}
