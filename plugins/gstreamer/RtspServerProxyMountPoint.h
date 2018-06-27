#pragma once

#include "MountPoint.h"


class RtspServerProxyMountPoint : public MountPoint
{
public:
    RtspServerProxyMountPoint(
        janus_callbacks*, janus_plugin*,
        const std::string& path,
        Flags,
        const std::string& description);
    ~RtspServerProxyMountPoint();

    void init();

protected:
    std::unique_ptr<Media> createMedia() override;

private:
    const std::string _path;
};
