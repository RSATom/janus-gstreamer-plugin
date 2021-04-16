#include "RtspMedia.h"

#include <string>
#include <algorithm>

#include <gst/app/gstappsink.h>

extern "C" {
#include "janus/debug.h"
}

#include "CxxPtr/GlibPtr.h"
#include "CxxPtr/GstPtr.h"

#define NO_MORE_PADS_MESSAGE "NO_MORE_PADS"


struct RtspMedia::Private
{
    RtspMedia *const owner;

    std::string mrl;

    GstElementPtr pipelinePtr;
    GstElement* rtspsrc;
    GstBusPtr busPtr;
    guint busWatchId = 0;

    GstSDPMessagePtr sdpPtr;

    void setState(GstState);

    void prepare();
    void pause();
    void play();
    void null();

    void postMessage(const gchar*);

    void onSdp(GstElement* rtspsrc, GstSDPMessage*);
    void rtspSrcPadAdded(GstElement* rtspsrc, GstPad*);
    void rtspNoMorePads(GstElement* rtspsrc);

    gboolean onBusMessage(GstBus*, GstMessage*);
};

void RtspMedia::Private::setState(GstState state)
{
    GstElement* pipeline = pipelinePtr.get();
    if(!pipeline) {
        if(state != GST_STATE_NULL)
            JANUS_LOG(LOG_ERR, "RtspMedia::Private::setState. Pipeline is not initialized\n");
        return;
    }

    switch(gst_element_set_state(pipeline, state)) {
        case GST_STATE_CHANGE_FAILURE:
            JANUS_LOG(LOG_ERR, "RtspMedia::Private::setState. gst_element_set_state failed\n");
            break;
        case GST_STATE_CHANGE_SUCCESS:
            break;
        case GST_STATE_CHANGE_ASYNC:
            break;
        case GST_STATE_CHANGE_NO_PREROLL:
            break;
    }
}

void RtspMedia::Private::prepare()
{
    pipelinePtr.reset(gst_pipeline_new(nullptr));
    GstElement* pipeline = pipelinePtr.get();

    GstElementPtr rtspsrcPtr(gst_element_factory_make("rtspsrc", nullptr));
    rtspsrc = rtspsrcPtr.get();
    if(!rtspsrc) {
        JANUS_LOG(LOG_ERR, "RtspMedia::Private::prepare. Fail create rtspsrc element\n");
        return;
    }

    auto onSdpCallback =
        (void (*)(GstElement*, GstSDPMessage*, gpointer))
        [] (GstElement* rtspsrc, GstSDPMessage* sdp, gpointer userData)
    {
        Private* self = static_cast<Private*>(userData);
        self->onSdp(rtspsrc, sdp);
    };
    g_signal_connect(rtspsrc, "on-sdp", G_CALLBACK(onSdpCallback), this);

    auto rtspSrcPadAddedCallback =
        (void (*)(GstElement*, GstPad*, gpointer))
         [] (GstElement* rtspsrc, GstPad* pad, gpointer userData)
    {
        Private* self = static_cast<Private*>(userData);
        self->rtspSrcPadAdded(rtspsrc, pad);
    };
    g_signal_connect(rtspsrc, "pad-added", G_CALLBACK(rtspSrcPadAddedCallback), this);

    auto rtspNoMorePadsCallback =
        (void (*)(GstElement*,  gpointer))
         [] (GstElement* rtspsrc, gpointer userData)
    {
        Private* self = static_cast<Private*>(userData);
        self->rtspNoMorePads(rtspsrc);
    };
    g_signal_connect(rtspsrc, "no-more-pads", G_CALLBACK(rtspNoMorePadsCallback), this);

    g_object_set(rtspsrc,
        "location", mrl.c_str(),
        nullptr);

    gst_bin_add(GST_BIN(pipeline), rtspsrcPtr.release());

    auto onBusMessageCallback =
        (gboolean (*) (GstBus*, GstMessage*, gpointer))
        [] (GstBus* bus, GstMessage* message, gpointer userData) -> gboolean
    {
        Private* self = static_cast<Private*>(userData);
        return self->onBusMessage(bus, message);
    };

    busPtr.reset(gst_pipeline_get_bus(GST_PIPELINE(pipeline)));
    GstBus* bus = busPtr.get();
    GSourcePtr busSourcePtr(gst_bus_create_watch(bus));
    busWatchId =
        gst_bus_add_watch(bus, onBusMessageCallback, this);
}

void RtspMedia::Private::pause()
{
    setState(GST_STATE_PAUSED);
}

void RtspMedia::Private::play()
{
    setState(GST_STATE_PLAYING);
}

void RtspMedia::Private::null()
{
    setState(GST_STATE_NULL);
}

void RtspMedia::Private::postMessage(const gchar* message)
{
    GstStructure* structure = gst_structure_new_empty(message);
    GstMessage* gstMessage = gst_message_new_application(NULL, structure);
    gst_bus_post(busPtr.get(), gstMessage);
}

void RtspMedia::Private::onSdp(GstElement* /*rtspsrc*/,
                               GstSDPMessage* sdp)
{
    GCharPtr sdpStrPtr(gst_sdp_message_as_text(sdp));
    JANUS_LOG(LOG_VERB, "RtspMedia::Private::OnSdp. \n%s\n", sdpStrPtr.get());

    GstSDPMessage* copy;
    if(GST_SDP_OK == gst_sdp_message_copy(sdp, &copy))
        sdpPtr.reset(copy);
    else
        JANUS_LOG(LOG_ERR, "RtspMedia::Private::OnSdp. gst_sdp_message_copy failed\n");
}

void RtspMedia::Private::rtspSrcPadAdded(
    GstElement* /*rtspsrc*/,
    GstPad* pad)
{
    GstCapsPtr capsPtr(gst_pad_get_current_caps(pad));
    GstCaps* caps = capsPtr.get();
    GCharPtr capsStrPtr(gst_caps_to_string(caps));
    JANUS_LOG(LOG_VERB, "Stream caps: %s\n", capsStrPtr.get());

    GstStructure* structure = gst_caps_get_structure(caps, 0);

    const gchar* media =
        gst_structure_get_string(structure, "media");

    StreamType streamType = StreamType::Unknown;
    if(0 == g_strcmp0(media, "video"))
        streamType = StreamType::Video;
    else if(0 == g_strcmp0(media, "audio"))
        streamType = StreamType::Audio;

    GstElement* streamSink = owner->addStream(streamType);
    if(!streamSink)
        return;

    gst_bin_add(GST_BIN(pipelinePtr.get()), streamSink);
    gst_element_set_state(streamSink, GST_STATE_PLAYING);

    GstPadPtr sinkPadPtr(gst_element_get_static_pad(streamSink, "sink"));
    GstPad* sinkPad = sinkPadPtr.get();

    if(sinkPad)
        gst_pad_link(pad, sinkPad);
}

void RtspMedia::Private::rtspNoMorePads(GstElement* /*rtspsrc*/)
{
    postMessage(NO_MORE_PADS_MESSAGE);
}

gboolean RtspMedia::Private::onBusMessage(GstBus* bus, GstMessage* msg)
{
    switch(GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            owner->eos(false);
            break;
        case GST_MESSAGE_ERROR: {
            gchar* debug;
            GError* error;

            gst_message_parse_error(msg, &error, &debug);

            JANUS_LOG(LOG_ERR, "RtspMedia::Private::onBusMessage. %s\n", error->message);

            g_free(debug);
            g_error_free(error);

            owner->eos(true);

           break;
        }
        case GST_MESSAGE_APPLICATION: {
            const GstStructure* structure = gst_message_get_structure(msg);
            const gchar* name = gst_structure_get_name(structure);
            if(0 == g_strcmp0(name, NO_MORE_PADS_MESSAGE)) {
                owner->prepared();
            }
            break;
        }
        default:
            break;
    }

    return TRUE;
}


RtspMedia::RtspMedia(const std::string& mrl) :
    _p(new Private{.owner = this, .mrl = mrl})
{
}

RtspMedia::~RtspMedia()
{
    shutdown();
    _p.reset();
}

const GstSDPMessage* RtspMedia::sdp() const
{
    return _p->sdpPtr.get();
}

void RtspMedia::doRun()
{
    _p->prepare();
    _p->pause();
    _p->play();
}

void RtspMedia::shutdown()
{
    _p->null();
}
