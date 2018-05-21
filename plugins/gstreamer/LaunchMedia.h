#pragma once

#include "Media.h"


class LaunchMedia : public Media
{
    LaunchMedia(const LaunchMedia&) = delete;
    LaunchMedia(LaunchMedia&&) = delete;
    LaunchMedia& operator = (const LaunchMedia&) = delete;

public:
    LaunchMedia(const std::string& pipeline);
    ~LaunchMedia();

    const GstSDPMessage* sdp() const override;

    void shutdown() override;

protected:
    void doRun() override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
