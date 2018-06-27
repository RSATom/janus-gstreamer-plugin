#include "Media.h"

#include <deque>
#include <algorithm>

#include "GstPtr.h"


struct Media::Private
{
    PreparedCallback preparedCallback;
    OnBufferCallback onBufferCallback;
    EosCallback eosCallback;

    struct Stream {
        Media::Stream stream;
        GstAppSink* sink;
    };
    std::deque<Stream> streams;

    inline int sinkIndex(GstAppSink* sink);

    GstFlowReturn onAppSinkPreroll(GstAppSink*);
    GstFlowReturn onAppSinkSample(GstAppSink*);
    void onAppSinkEos(GstAppSink*);
};

int Media::Private::sinkIndex(GstAppSink* sink)
{
    auto it =
        std::find_if(
            streams.begin(), streams.end(),
                [sink] (const Stream& stream) -> bool {
                    return sink == stream.sink;
                }
            );

    if(it == streams.end())
        return -1;
    else
        return it - streams.begin();
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


Media::Media() :
    _p(new Private)
{
}

Media::~Media()
{
    _p.reset();
}

bool Media::hasSdp() const
{
    return nullptr != sdp();
}

unsigned Media::streamsCount() const
{
    return _p->streams.size();
}

std::vector<Media::Stream> Media::streams() const
{
    std::vector<Media::Stream> streams;
    streams.reserve(_p->streams.size());

    for(Private::Stream& stream: _p->streams)
        streams.emplace_back(stream.stream);

    return std::move(streams);
}

void Media::run(
    const PreparedCallback& prepared,
    const OnBufferCallback& onBuffer,
    const EosCallback& eos)
{
    _p->preparedCallback = prepared;
    _p->onBufferCallback = onBuffer;
    _p->eosCallback = eos;

    doRun();
}

void Media::addStream(StreamType streamType, GstAppSink* appSink)
{
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
        _p.get(),
        nullptr);

    _p->streams.emplace_back(Private::Stream{{streamType}, appSink});
}

GstElement* Media::addStream(StreamType streamType)
{
    GstElementPtr appSinkPtr(gst_element_factory_make("appsink", nullptr));
    GstAppSink* appSink = GST_APP_SINK(appSinkPtr.get());

    if(!appSink)
        return nullptr;

    addStream(streamType, appSink);

    return appSinkPtr.release();
}

void Media::prepared()
{
    if(_p->preparedCallback)
        _p->preparedCallback();
}

void Media::eos(bool error)
{
    if(_p->eosCallback)
        _p->eosCallback(error);
}
