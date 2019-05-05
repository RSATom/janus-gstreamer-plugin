#include "ConfigLoader.h"

extern "C" {
#include "janus/utils.h"
}

#include "PluginConfig.h"
#include "RtspMountPoint.h"
#include "LaunchMountPoint.h"

#include "GlibPtr.h"

#define USE_CONFIG 1


void LoadConfig(
    janus_callbacks* janus,
    janus_plugin* janusPlugin,
    const std::string& configFile,
    PluginConfig* pluginConfig,
    std::map<int, std::unique_ptr<MountPoint>>* mountPoints)
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

    janus_config_item* enableDynamicMountPointsItem =
        janus_config_get(config, general, janus_config_type_item, "enable_dynamic_mount_points");

    pluginConfig->enableDynamicMountPoints =
        enableDynamicMountPointsItem &&
        enableDynamicMountPointsItem->value &&
        janus_is_true(enableDynamicMountPointsItem->value);


    janus_config_array* streamsList =
        janus_config_get(config, NULL, janus_config_type_array, "streams");

    GListPtr streamsPtr(
        janus_config_get_categories(config, streamsList));

    GList* streamItem = streamsPtr.get();
    for(; streamItem; streamItem = g_list_next(streamItem)) {
        janus_config_category* stream =
            static_cast<janus_config_category*>(streamItem->data);

        janus_config_item* descriptionItem =
            janus_config_get(config, stream, janus_config_type_item, "description");
        janus_config_item* typeItem =
            janus_config_get(config, stream, janus_config_type_item, "type");
        janus_config_item* videoItem =
            janus_config_get(config, stream, janus_config_type_item, "video");
        janus_config_item* audioItem =
            janus_config_get(config, stream, janus_config_type_item, "audio");

        if(!typeItem || !typeItem->value)
            continue;

        const std::string description =
            descriptionItem && descriptionItem->value ?
                std::string(descriptionItem->value) :
                std::string();

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

        const std::string type = typeItem->value;
        if(type == "rtsp") {
            janus_config_item* urlItem =
                janus_config_get(config, stream, janus_config_type_item, "url");

            if(!urlItem || !urlItem->value)
                continue;

            const std::string url = urlItem->value;
            if(url.empty())
                continue;

            mountPoints->emplace(
                mountPoints->size() + 1,
                new RtspMountPoint(
                    janus, janusPlugin,
                    url,
                    flags,
                    description.empty() ? url : description)
                );
        } else if(type == "launch") {
            janus_config_item* pipelineItem =
                janus_config_get(config, stream, janus_config_type_item, "pipeline");

            if(!pipelineItem || !pipelineItem->value)
                continue;

            const std::string pipeline = pipelineItem->value;
            if(pipeline.empty())
                continue;

            mountPoints->emplace(
                mountPoints->size() + 1,
                new LaunchMountPoint(
                    janus, janusPlugin,
                    pipeline,
                    flags,
                    description.empty() ? pipeline : description)
                );
        } else
            continue;
    }
#else
    mountPoints->emplace(
        1,
        std::make_unique<RtspMountPoint>(
            janus, janusPlugin,
            "rtsp://restream-basic.eastasia.cloudapp.azure.com:8090/bars",
            MountPoint::RESTREAM_BOTH,
            "rtsp://restream-basic.eastasia.cloudapp.azure.com:8090/bars")
        );
    mountPoints->emplace(
        2,
        std::make_unique<RtspMountPoint>(
            janus, janusPlugin,
            "rtsp://restream-basic.eastasia.cloudapp.azure.com:8100/bars",
            MountPoint::RESTREAM_BOTH,
            "rtsp://restream-basic.eastasia.cloudapp.azure.com:8100/bars")
        );
    mountPoints->emplace(
        3,
        std::make_unique<RtspMountPoint>(
            janus, janusPlugin,
            "rtsp://restream-basic.eastasia.cloudapp.azure.com:8100/dlink931",
            MountPoint::RESTREAM_BOTH,
            "rtsp://restream-basic.eastasia.cloudapp.azure.com:8100/dlink931")
        );
    mountPoints->emplace(
        4,
        std::make_unique<RtspMountPoint>(
            janus, janusPlugin,
            "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_115k.mov",
            MountPoint::RESTREAM_VIDEO,
            "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_115k.mov")
        );
    mountPoints->emplace(
        5,
        std::make_unique<LaunchMountPoint>(
            janus, janusPlugin,
            "videotestsrc pattern=blue ! "
            "clockoverlay halignment=center valignment=center shaded-background=true font-desc=\"Sans, 36\" ! "
            "x264enc ! video/x-h264, profile=baseline ! rtph264pay pt=99 config-interval=1 name=videopay",
            MountPoint::RESTREAM_VIDEO,
            "Clock")
        );
#endif
}
