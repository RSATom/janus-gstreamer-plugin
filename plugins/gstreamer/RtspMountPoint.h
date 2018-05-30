#pragma once

#include "MountPoint.h"


class RtspMountPoint : public MountPoint
{
public:
    RtspMountPoint(
        janus_callbacks*, janus_plugin*,
        const std::string& mrl,
        Flags,
        const std::string& description);

protected:
    std::unique_ptr<Media> createMedia() override;

private:
    const std::string _mrl;
};
