#pragma once

#include "MountPoint.h"


class LaunchMountPoint : public MountPoint
{
public:
    LaunchMountPoint(
        janus_callbacks*, janus_plugin*,
        const std::string& pipeline,
        Flags,
        const std::string& description);

protected:
    std::unique_ptr<Media> createMedia() override;

private:
    const std::string _pipeline;
};
