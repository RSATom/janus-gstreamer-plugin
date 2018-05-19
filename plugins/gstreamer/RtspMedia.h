#pragma once

#include "Media.h"


class RtspMedia : public Media
{
    RtspMedia(const RtspMedia&) = delete;
    RtspMedia(RtspMedia&&) = delete;
    RtspMedia& operator = (const RtspMedia&) = delete;

public:
    RtspMedia(const std::string& mrl);
    ~RtspMedia();

    const GstSDPMessage* sdp() const override;

    void shutdown() override;

protected:
    void doRun() override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
