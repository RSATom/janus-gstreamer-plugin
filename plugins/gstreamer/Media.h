#pragma once

#include <memory>
#include <functional>
#include <vector>

#include <glib.h>

#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>
#include <gst/app/gstappsink.h>


class Media
{
    Media(const Media&) = delete;
    Media(Media&&) = delete;
    Media& operator = (const Media&) = delete;

public:
    enum class StreamType {
        Unknown,
        Video,
        Audio
    };

    struct Stream {
        StreamType type;
    };

    Media();
    virtual ~Media();

    bool hasSdp() const;
    virtual const GstSDPMessage* sdp() const = 0;

    unsigned streamsCount() const;
    std::vector<Stream> streams() const;

    typedef std::function<void ()> PreparedCallback;
    typedef std::function<void (int stream, const void* data, gsize size)> OnBufferCallback;
    typedef std::function<void (bool error)> EosCallback;
    // OnBufferCallback will be called from a streaming thread
    void run(const PreparedCallback&, const OnBufferCallback&, const EosCallback&);
    virtual void shutdown() = 0;

protected:
    virtual void doRun() = 0;

    void addStream(StreamType streamType, GstAppSink*);
    GstElement* addStream(StreamType);

    void prepared();
    void eos(bool error);

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
