#pragma once

#include <memory>
#include <functional>

#include <glib.h>

#include <gst/sdp/gstsdpmessage.h>


class Media
{
    Media(const Media&) = delete;
    Media(Media&&) = delete;
    Media& operator = (const Media&) = delete;

public:
    Media(const std::string& mrl, GMainLoop*);
    ~Media();

    bool hasSdp() const;
    const GstSDPMessage* sdp() const;
    unsigned streamsCount() const;

    typedef std::function<void ()> PreparedCallback;
    typedef std::function<void (int stream, const void* data, gsize size)> OnBufferCallback;
    // OnBufferCallback will be called from a streaming thread
    void run(const PreparedCallback&, const OnBufferCallback&);

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
