#pragma once

extern "C" {
#include "janus/plugins/plugin.h"
}


void StartPluginThread();

void PostClientMessage(
    janus_plugin_session*,
    char* transaction,
    json_t* message);

void PostHangupMessage(
    janus_plugin_session*);

void PostDestroyMessage(
    janus_plugin_session*);
