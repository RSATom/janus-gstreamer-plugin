#pragma once

extern "C" {
#include "janus/plugins/plugin.h"
}


void StartPluginThread();

void PostPluginMessage(
    janus_plugin_session* janusSession,
    char* transaction,
    json_t* message,
    bool destroy);
