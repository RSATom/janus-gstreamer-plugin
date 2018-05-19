#include "ConfigLoader.h"

extern "C" {
#include "janus/utils.h"
}

#include "MountPoint.h"

#include "GlibPtr.h"

// #define USE_CONFIG 1


void LoadConfig(
    janus_callbacks* janus,
    janus_plugin* janusPlugin,
    const std::string& configFile,
    std::map<int, MountPoint>* mountPoints)
{
#if USE_CONFIG
    JanusConfigPtr configPtr(janus_config_parse(configFile.c_str()));
    janus_config* config = configPtr.get();
    if(!config) {
        JANUS_LOG(LOG_ERR, "Failed to load config file \"%s\"\n", configFile.c_str());
        return;
    }

    janus_config_category* general =
        janus_config_get(config, NULL, janus_config_type_category, "general");

    // nothing to load from general atm

    janus_config_array* streamsList =
        janus_config_get(config, NULL, janus_config_type_array, "streams");

    GListPtr streamsPtr(
        janus_config_get_categories(config, streamsList));

    GList* streamItem = streamsPtr.get();
    for(; streamItem; streamItem = g_list_next(streamItem)) {
        janus_config_category* stream =
            static_cast<janus_config_category*>(streamItem->data);

        janus_config_item* typeItem =
            janus_config_get(config, stream, janus_config_type_item, "type");
        janus_config_item* urlItem =
            janus_config_get(config, stream, janus_config_type_item, "url");
        janus_config_item* videoItem =
            janus_config_get(config, stream, janus_config_type_item, "video");
        janus_config_item* audioItem =
            janus_config_get(config, stream, janus_config_type_item, "audio");

        if(!typeItem || !typeItem->value)
            continue;
        if(!urlItem || !urlItem->value)
            continue;

        const std::string type = typeItem->value;
        if(type != "rtsp")
            continue;

        const std::string url = urlItem->value;
        if(url.empty())
            continue;

        const bool video =
            !videoItem || !videoItem->value || janus_is_true(videoItem->value);
        const bool audio =
            !audioItem || !audioItem->value || janus_is_true(audioItem->value);

        MountPoint::Flags flags;
        if(video && audio)
            flags = MountPoint::RESTREAM_BOTH;
        else if(video)
            flags = MountPoint::RESTREAM_VIDEO;
        else if(audio)
            flags = MountPoint::RESTREAM_AUDIO;
        else
            continue;

        mountPoints->emplace(
            std::piecewise_construct,
            std::make_tuple(mountPoints->size() + 1),
            std::make_tuple(
                janus, janusPlugin,
                url, flags)
            );
    }
#else
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
#endif
}