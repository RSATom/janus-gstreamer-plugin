#include "LaunchMedia.h"

#include <algorithm>
#include <vector>
#include <mutex>
#include <set>

#include <gst/app/gstappsink.h>

extern "C" {
#include "janus/debug.h"
}

#include "CxxPtr/GlibPtr.h"
#include "CxxPtr/GstPtr.h"

#define ALL_PADS_PREROLLED_MESSAGE "PADS_PREROLLED"


struct LaunchMedia::Private
{
    LaunchMedia *const owner;

    std::string pipelineDesc;

    GstElementPtr pipelinePtr;

    GstBusPtr busPtr;
    guint busWatchId = 0;

    struct Stream {
        GstElementPtr payloaderPtr;
    };
    std::vector<Stream> streams;

    std::mutex waitingCapsPadsGuard;
    std::set<GstPad*> waitingCapsPads;

    GstSDPMessagePtr sdpPtr;

    void setState(GstState);

    void prepare();
    void pause();
    void play();
    void null();

    gboolean onBusMessage(GstBus*, GstMessage*);
    void prerolled();

    void postMessage(const gchar*);
};

void LaunchMedia::Private::postMessage(const gchar* message)
{
    GstStructure* structure = gst_structure_new_empty(message);
    GstMessage* gstMessage = gst_message_new_application(NULL, structure);
    gst_bus_post(busPtr.get(), gstMessage);
}

void LaunchMedia::Private::setState(GstState state)
{
    GstElement* pipeline = pipelinePtr.get();
    if(!pipeline) {
        if(state != GST_STATE_NULL)
            JANUS_LOG(LOG_ERR, "LaunchMedia::Private::setState. Pipeline is not initialized\n");
        return;
    }

    switch(gst_element_set_state(pipeline, state)) {
        case GST_STATE_CHANGE_FAILURE:
            JANUS_LOG(LOG_ERR, "LaunchMedia::Private::setState. gst_element_set_state failed\n");
            break;
        case GST_STATE_CHANGE_SUCCESS:
            break;
        case GST_STATE_CHANGE_ASYNC:
            break;
        case GST_STATE_CHANGE_NO_PREROLL:
            break;
    }
}

void LaunchMedia::Private::prepare()
{
    GError* parseError = nullptr;
    pipelinePtr.reset(gst_parse_launch(pipelineDesc.c_str(), &parseError));
    GErrorPtr parseErrorPtr(parseError);
    GstElement* pipeline = pipelinePtr.get();
    if(parseError) {
        JANUS_LOG(LOG_ERR,
            "LaunchMedia::Private::prepare. gst_parse_launch failed: %s\n",
            parseError->message);
        return;
    }

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

    auto padPrerolled =
        (GstPadProbeReturn (*) (GstPad*, GstPadProbeInfo*, gpointer))
        [] (GstPad* pad, GstPadProbeInfo* /*info*/, gpointer userData) -> GstPadProbeReturn {
            Private* self = static_cast<Private*>(userData);

            GstCapsPtr capsPtr(gst_pad_get_current_caps(pad));
            GstCaps* caps = capsPtr.get();
            if(!caps) // FIXME! why?
                return GST_PAD_PROBE_OK;

            std::lock_guard<std::mutex> lock(self->waitingCapsPadsGuard);
            const bool removed = self->waitingCapsPads.erase(pad) != 0;

            if(removed && self->waitingCapsPads.empty())
                self->postMessage(ALL_PADS_PREROLLED_MESSAGE);

            return GST_PAD_PROBE_REMOVE;
        };

    auto addStreamSink =
        [this, pipeline, padPrerolled] (GstElementPtr& payloaderPtr, StreamType streamType) {
            if(!payloaderPtr)
                return;

            GstElement* payloader = payloaderPtr.get();
            GstPadPtr payloaderPadPtr(gst_element_get_static_pad(payloader, "src"));
            GstPad* payloaderPad = payloaderPadPtr.get();

            GstElement* streamSink = owner->addStream(streamType);
            if(streamSink) {
                gst_bin_add(GST_BIN(pipeline), streamSink);

                GstPadPtr sinkPadPtr(gst_element_get_static_pad(streamSink, "sink"));
                GstPad* sinkPad = sinkPadPtr.get();

                if(sinkPad) {
                    gst_pad_link(payloaderPad, sinkPad);
                    gst_object_ref(payloader);
                    streams.push_back(Stream{GstElementPtr(payloader)});

                    waitingCapsPads.insert(sinkPad);
                    gst_pad_add_probe(sinkPad, GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM,
                        padPrerolled, this, NULL);
                }
            }
        };

    GstElementPtr videoPayloaderPtr(
        gst_bin_get_by_name(GST_BIN(pipeline), "videopay"));
    GstElementPtr audioPayloaderPtr(
        gst_bin_get_by_name(GST_BIN(pipeline), "audiopay"));

    if(videoPayloaderPtr)
        addStreamSink(videoPayloaderPtr, StreamType::Video);

    if(audioPayloaderPtr)
        addStreamSink(videoPayloaderPtr, StreamType::Audio);
}

void LaunchMedia::Private::pause()
{
    setState(GST_STATE_PAUSED);
}

void LaunchMedia::Private::play()
{
    setState(GST_STATE_PLAYING);
}

void LaunchMedia::Private::null()
{
    setState(GST_STATE_NULL);
}

void LaunchMedia::Private::prerolled()
{
    GstSDPMessage* outSdp;
    gst_sdp_message_new(&outSdp);
    GstSDPMessagePtr outSdpPtr(outSdp);

    for(Stream& stream: streams) {
        GstElement* payloader = stream.payloaderPtr.get();
        GstPadPtr payloaderPadPtr(gst_element_get_static_pad(payloader, "src"));
        GstPad* payloaderPad = payloaderPadPtr.get();

        GstCapsPtr capsPtr(gst_pad_get_current_caps(payloaderPad));
        GstCaps* caps = capsPtr.get();
        GCharPtr capsStrPtr(gst_caps_to_string(caps));
        JANUS_LOG(LOG_VERB, "Stream caps: %s\n", capsStrPtr.get());

        GstSDPMedia* outMedia;
        gst_sdp_media_new(&outMedia);
        GstSDPMediaPtr outMediaPtr(outMedia);

        gst_sdp_media_set_media_from_caps(caps, outMedia);

        gst_sdp_message_add_media(outSdp, outMediaPtr.release());
    }

    sdpPtr = std::move(outSdpPtr);

    owner->prepared();
}

gboolean LaunchMedia::Private::onBusMessage(GstBus* bus, GstMessage* msg)
{
    switch(GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            owner->eos(false);
            break;
        case GST_MESSAGE_ERROR: {
            gchar* debug;
            GError* error;

            gst_message_parse_error(msg, &error, &debug);

            JANUS_LOG(LOG_ERR, "LaunchMedia::Private::onBusMessage. %s\n", error->message);

            g_free(debug);
            g_error_free(error);

            owner->eos(true);
           break;
        }
        case GST_MESSAGE_APPLICATION: {
            const GstStructure* structure = gst_message_get_structure(msg);
            const gchar* name = gst_structure_get_name(structure);
            if(0 == g_strcmp0(name, ALL_PADS_PREROLLED_MESSAGE))
                prerolled();

            break;
        }
        default:
            break;
    }

    return TRUE;
}


LaunchMedia::LaunchMedia(const std::string& pipeline) :
    _p(new Private{.owner = this, .pipelineDesc = pipeline})
{
}

LaunchMedia::~LaunchMedia()
{
    shutdown();
    _p.reset();
}

const GstSDPMessage* LaunchMedia::sdp() const
{
    return _p->sdpPtr.get();
}

void LaunchMedia::doRun()
{
    _p->prepare();
    _p->pause();
    _p->play();
}

void LaunchMedia::shutdown()
{
    _p->null();
}
