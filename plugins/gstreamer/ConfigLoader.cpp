#include "ConfigLoader.h"

#include "MountPoint.h"


void LoadConfig(
    janus_callbacks* janus,
    janus_plugin* janusPlugin,
    const char* /*configPath*/,
    std::map<int, MountPoint>* mountPoints)
{
    mountPoints->emplace(
        std::piecewise_construct,
        std::make_tuple(1),
        std::make_tuple(
            janus, janusPlugin,
            "rtsp://restream-basic.eastasia.cloudapp.azure.com:8090/bars",
            MountPoint::RESTREAM_BOTH)
        );
    mountPoints->emplace(
        std::piecewise_construct,
        std::make_tuple(2),
        std::make_tuple(
            janus, janusPlugin,
            "rtsp://restream-basic.eastasia.cloudapp.azure.com:8100/bars",
            MountPoint::RESTREAM_BOTH)
        );
    mountPoints->emplace(
        std::piecewise_construct,
        std::make_tuple(3),
        std::make_tuple(
            janus, janusPlugin,
            "rtsp://restream-basic.eastasia.cloudapp.azure.com:8100/dlink931",
            MountPoint::RESTREAM_BOTH)
        );
    mountPoints->emplace(
        std::piecewise_construct,
        std::make_tuple(4),
        std::make_tuple(
            janus, janusPlugin,
            "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_115k.mov",
            MountPoint::RESTREAM_VIDEO)
        );
}
