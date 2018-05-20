#include "RtspMountPoint.h"

#include "RtspMedia.h"


RtspMountPoint::RtspMountPoint(
    janus_callbacks* janus, janus_plugin* plugin,
    const std::string& mrl,
    Flags flags) :
    MountPoint(janus, plugin, flags, mrl),
    _mrl(mrl)
{
}

std::unique_ptr<Media> RtspMountPoint::createMedia()
{
    return std::unique_ptr<Media>(new RtspMedia(_mrl));
}
