#pragma once

#include <memory>

#include <gst/rtsp-server/rtsp-server.h>


class RtspServer
{
public:
    RtspServer();
    ~RtspServer();

    void attach();

    // takes ownership over media factory
    void addMountPoint(const std::string& path, GstRTSPMediaFactory*);

private:
    void initServer();

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
