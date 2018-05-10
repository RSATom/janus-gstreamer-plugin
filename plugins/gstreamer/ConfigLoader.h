#pragma once

#include <map>

extern "C" {
#include "janus/plugins/plugin.h"
}

class MountPoint; // #include "MountPoint.h"


void LoadConfig(
    janus_callbacks* janus,
    janus_plugin* janusPlugin,
    const char* /*configPath*/,
    std::map<int, MountPoint>* mountPoints);
