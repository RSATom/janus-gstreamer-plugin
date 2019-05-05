#pragma once

#include <map>
#include <thread>

extern "C" {
#include "janus/plugins/plugin.h"
}

#include "CxxPtr/GlibPtr.h"
#include "PluginConfig.h"
#include "QueueSource.h"
#include "MountPoint.h"


struct PluginContext
{
    std::unique_ptr<janus_plugin> janusPlugin;
    janus_callbacks* janus;

    PluginConfig config;

    GMainContextPtr mainContextPtr;
    GMainLoopPtr loopPtr;
    QueueSourcePtr queueSourcePtr;
    std::thread mainThread;

    std::map<int, std::unique_ptr<MountPoint>> mountPoints;
    std::map<std::string, std::unique_ptr<MountPoint>> dynamicMountPoints;
};

PluginContext& Context();

inline const char* GetPluginName()
    { return Context().janusPlugin->get_name(); }
