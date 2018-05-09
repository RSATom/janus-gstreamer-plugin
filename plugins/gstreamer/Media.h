#pragma once

#include <memory>
#include <functional>
#include <vector>

#include <glib.h>

#include <gst/sdp/gstsdpmessage.h>


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
        int payload;
    };

    Media(const std::string& mrl);
    ~Media();

    bool hasSdp() const;
    const GstSDPMessage* sdp() const;

    unsigned streamsCount() const;
    std::vector<Stream> streams() const;

    typedef std::function<void ()> PreparedCallback;
    typedef std::function<void (int stream, const void* data, gsize size)> OnBufferCallback;
    typedef std::function<void (bool error)> EosCallback;
    // OnBufferCallback will be called from a streaming thread
    void run(const PreparedCallback&, const OnBufferCallback&, const EosCallback&);
    void shutdown();

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
