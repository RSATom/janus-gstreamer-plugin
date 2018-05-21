#include "LaunchMountPoint.h"

#include "LaunchMedia.h"


LaunchMountPoint::LaunchMountPoint(
    janus_callbacks* janus, janus_plugin* plugin,
    const std::string& pipeline,
    Flags flags,
    const std::string& description) :
    MountPoint(janus, plugin, flags, description),
    _pipeline(pipeline)
{
}

std::unique_ptr<Media> LaunchMountPoint::createMedia()
{
    return std::unique_ptr<Media>(new LaunchMedia(_pipeline));
}
