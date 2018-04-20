#include "Media.h"

#include <deque>
#include <algorithm>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

extern "C" {
#include "janus/debug.h"
}

#include "GlibPtr.h"
#include "GstPtr.h"

#define NO_MORE_PADS_MESSAGE "NO_MORE_PADS"


struct Media::Private
{
    std::string mrl;

    PreparedCallback preparedCallback;
    OnBufferCallback onBufferCallback;
    EosCallback eosCallback;

    GstElementPtr pipelinePtr;
    GstElement* rtspsrc;
    GstBusPtr busPtr;
    guint busWatchId = 0;

    GstSDPMessagePtr sdpPtr;

    std::deque<GstAppSink*> rtpSinks;

    void setState(GstState);

    void prepare();
    void pause();
    void play();
    void null();

    void postMessage(const gchar*);

    void onSdp(GstElement* rtspsrc, GstSDPMessage*);
    void rtspSrcPadAdded(GstElement* rtspsrc, GstPad*);
    void rtspNoMorePads(GstElement* rtspsrc);

    inline int sinkIndex(GstAppSink* sink);

    GstFlowReturn onAppSinkPreroll(GstAppSink*);
    GstFlowReturn onAppSinkSample(GstAppSink*);
    void onAppSinkEos(GstAppSink*);

    gboolean onBusMessage(GstBus*, GstMessage*);
};

void Media::Private::setState(GstState state)
{
    GstElement* pipeline = pipelinePtr.get();
    if(!pipeline) {
        if(state != GST_STATE_NULL)
            JANUS_LOG(LOG_ERR, "Media::Private::setState. Pipeline is not initialized\n");
        return;
    }

    switch(gst_element_set_state(pipeline, state)) {
        case GST_STATE_CHANGE_FAILURE:
            JANUS_LOG(LOG_ERR, "Media::Private::setState. gst_element_set_state failed\n");
            break;
        case GST_STATE_CHANGE_SUCCESS:
            break;
        case GST_STATE_CHANGE_ASYNC:
            break;
        case GST_STATE_CHANGE_NO_PREROLL:
            break;
    }
}

void Media::Private::prepare()
{
    pipelinePtr.reset(gst_pipeline_new(nullptr));
    GstElement* pipeline = pipelinePtr.get();

    GstElementPtr rtspsrcPtr(gst_element_factory_make("rtspsrc", nullptr));
    rtspsrc = rtspsrcPtr.get();

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
    g_source_set_callback(
        busSourcePtr.get(),
        (GSourceFunc) onBusMessageCallback,
        this, nullptr);
    busWatchId =
        g_source_attach(busSourcePtr.get(), g_main_context_get_thread_default());
}

void Media::Private::pause()
{
    setState(GST_STATE_PAUSED);
}

void Media::Private::play()
{
    setState(GST_STATE_PLAYING);
}

void Media::Private::null()
{
    setState(GST_STATE_NULL);
}

void Media::Private::postMessage(const gchar* message)
{
    GstStructure* structure = gst_structure_new_empty(message);
    GstMessage* gstMessage = gst_message_new_application(NULL, structure);
    gst_bus_post(busPtr.get(), gstMessage);
}

void Media::Private::onSdp(GstElement* /*rtspsrc*/,
                           GstSDPMessage* sdp)
{
    GCharPtr sdpStrPtr(gst_sdp_message_as_text(sdp));
    JANUS_LOG(LOG_VERB, "Media::Private::OnSdp. \n%s\n", sdpStrPtr.get());

    GstSDPMessage* copy;
    if(GST_SDP_OK == gst_sdp_message_copy(sdp, &copy))
        sdpPtr.reset(copy);
    else
        JANUS_LOG(LOG_ERR, "Media::Private::OnSdp. gst_sdp_message_copy failed\n");
}

void Media::Private::rtspSrcPadAdded(
    GstElement* /*rtspsrc*/,
    GstPad* pad)
{
    GstElement* pipeline = pipelinePtr.get();

    GstElementPtr appSinkPtr(gst_element_factory_make("appsink", nullptr));
    GstElement* sink = appSinkPtr.get();
    GstAppSink* appSink = GST_APP_SINK(sink);

    gst_app_sink_set_drop(appSink, TRUE);

    auto onAppSinkEosCallback =
        [] (GstAppSink* appsink, gpointer userData)
    {
        Private* self = static_cast<Private*>(userData);
    };
    auto onAppSinkPrerollCallback =
        [] (GstAppSink* appsink, gpointer userData) -> GstFlowReturn
    {
        Private* self = static_cast<Private*>(userData);
        return self->onAppSinkPreroll(appsink);
    };
    auto onAppSinkSampleCallback =
        [] (GstAppSink* appsink, gpointer userData) -> GstFlowReturn
    {
        Private* self = static_cast<Private*>(userData);
        return self->onAppSinkSample(appsink);
    };

    GstAppSinkCallbacks callbacks =
        {onAppSinkEosCallback, onAppSinkPrerollCallback, onAppSinkSampleCallback};
    gst_app_sink_set_callbacks(
        appSink,
        &callbacks,
        this,
        nullptr);

    gst_bin_add(GST_BIN(pipeline), appSinkPtr.release());
    gst_element_set_state(sink, GST_STATE_PLAYING);

    GstPadPtr sinkPadPtr(gst_element_get_static_pad(sink, "sink"));
    GstPad* sinkPad = sinkPadPtr.get();

    gst_pad_link(pad, sinkPad);

    rtpSinks.push_back(appSink);
}

void Media::Private::rtspNoMorePads(GstElement* /*rtspsrc*/)
{
    postMessage(NO_MORE_PADS_MESSAGE);
}

int Media::Private::sinkIndex(GstAppSink* sink)
{
    auto it = std::find(rtpSinks.begin(), rtpSinks.end(), sink);
    if(it == rtpSinks.end())
        return -1;
    else
        return it - rtpSinks.begin();
}

GstFlowReturn Media::Private::onAppSinkPreroll(GstAppSink* appsink)
{
    GstSamplePtr(gst_app_sink_pull_preroll(appsink));

    return GST_FLOW_OK;
}

GstFlowReturn Media::Private::onAppSinkSample(GstAppSink* appsink)
{
    if(!onBufferCallback)
        return GST_FLOW_OK;

    GstSamplePtr samplePtr(gst_app_sink_pull_sample(appsink));
    GstSample* sample = samplePtr.get();
    GstBuffer* buffer = gst_sample_get_buffer(sample);

    GstMapInfo mapInfo;

    gst_buffer_map(buffer,
                   &mapInfo,
                   GST_MAP_READ);

    onBufferCallback(sinkIndex(appsink), mapInfo.data, mapInfo.size);

    gst_buffer_unmap(buffer, &mapInfo);

    return GST_FLOW_OK;
}

void Media::Private::onAppSinkEos(GstAppSink* /*appsink*/)
{
}

gboolean Media::Private::onBusMessage(GstBus* bus, GstMessage* msg)
{
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            if(eosCallback)
                eosCallback(false);
            break;
        case GST_MESSAGE_ERROR: {
            gchar* debug;
            GError* error;

            gst_message_parse_error(msg, &error, &debug);

            JANUS_LOG(LOG_ERR, "Media::Private::onBusMessage. %s\n", error->message);

            g_free(debug);
            g_error_free(error);

            if(eosCallback)
                eosCallback(true);

           break;
        }
        case GST_MESSAGE_STATE_CHANGED:
            GstState oldState, newState, pendingState;
            gst_message_parse_state_changed(msg, &oldState, &newState, &pendingState);
            break;
        case GST_MESSAGE_APPLICATION: {
            const GstStructure* structure = gst_message_get_structure(msg);
            const gchar* name = gst_structure_get_name(structure);
            if(0 == g_strcmp0(name, NO_MORE_PADS_MESSAGE)) {
                if(preparedCallback)
                    preparedCallback();
            }
            break;
        }
        default:
            break;
    }

    return TRUE;
}


Media::Media(const std::string& mrl) :
    _p(new Private{.mrl = mrl})
{
}

Media::~Media()
{
    shutdown();
    _p.reset();
}

bool Media::hasSdp() const
{
    return nullptr != _p->sdpPtr;
}

const GstSDPMessage* Media::sdp() const
{
    return _p->sdpPtr.get();
}

unsigned Media::streamsCount() const
{
    return _p->rtpSinks.size();
}

void Media::run(
    const PreparedCallback& prepared,
    const OnBufferCallback& onBuffer,
    const EosCallback& eos)
{
    _p->preparedCallback = prepared;
    _p->onBufferCallback = onBuffer;
    _p->eosCallback = eos;

    _p->prepare();
    _p->pause();
    _p->play();
}

void Media::shutdown()
{
    _p->null();
}
