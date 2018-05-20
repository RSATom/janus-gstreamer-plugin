#pragma once

#include <memory>
#include <map>

extern "C" {
#include "janus/plugins/plugin.h"
}

class MountPoint; // #include "MountPoint.h"


void LoadConfig(
    janus_callbacks* janus,
    janus_plugin* janusPlugin,
    const std::string& configFile,
    std::map<int, std::unique_ptr<MountPoint>>* mountPoints);
