#pragma once

#include "Media.h"


class RtspServerProxyMedia : public Media
{
    RtspServerProxyMedia(const RtspServerProxyMedia&) = delete;
    RtspServerProxyMedia(RtspServerProxyMedia&&) = delete;
    RtspServerProxyMedia& operator = (const RtspServerProxyMedia&) = delete;

public:
    RtspServerProxyMedia();
    ~RtspServerProxyMedia();

    const GstSDPMessage* sdp() const override;

    void shutdown() override;

protected:
    void doRun() override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
