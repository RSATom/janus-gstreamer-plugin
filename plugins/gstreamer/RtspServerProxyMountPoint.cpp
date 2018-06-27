#include "RtspServerProxyMountPoint.h"

#include "PluginContext.h"
#include "RtspServerProxyMedia.h"
#include "RtspServerMediaFactory.h"


RtspServerProxyMountPoint::RtspServerProxyMountPoint(
    janus_callbacks* janus, janus_plugin* plugin,
    const std::string& path,
    Flags flags,
    const std::string& description) :
    MountPoint(janus, plugin, flags, description),
    _path(path)
{
}

RtspServerProxyMountPoint::~RtspServerProxyMountPoint()
{
}

void RtspServerProxyMountPoint::init()
{
    PluginContext& context = Context();
    context.rtspServer->addMountPoint(
        _path,
        GST_RTSP_MEDIA_FACTORY(rtsp_server_media_factory_new(this)));
}

std::unique_ptr<Media> RtspServerProxyMountPoint::createMedia()
{
    return std::unique_ptr<Media>(new RtspServerProxyMedia());
}
