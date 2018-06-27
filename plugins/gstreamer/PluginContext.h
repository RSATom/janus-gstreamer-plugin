#pragma once

#include <map>
#include <thread>

extern "C" {
#include "janus/plugins/plugin.h"
}

#include "GlibPtr.h"
#include "QueueSource.h"
#include "MountPoint.h"
#include "RtspServer.h"


struct PluginContext
{
    std::unique_ptr<janus_plugin> janusPlugin;
    janus_callbacks* janus;

    GMainContextPtr mainContextPtr;
    GMainLoopPtr loopPtr;
    QueueSourcePtr queueSourcePtr;
    std::thread mainThread;

    std::map<int, std::unique_ptr<MountPoint>> mountPoints;

    std::unique_ptr<RtspServer> rtspServer;
};

PluginContext& Context();

inline const char* GetPluginName()
    { return Context().janusPlugin->get_name(); }
