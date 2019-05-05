#pragma once

#include "CxxPtr/GlibPtr.h"

#include "MountPoint.h"


struct Session
{
    MountPoint* watching;
    bool dynamicMountPointWatching;
    GCharPtr sdpSessionId;
};

inline Session* GetSession(janus_plugin_session* janusSession)
    { return static_cast<Session*>(janusSession->plugin_handle); }

void PushError(
    janus_callbacks* janus,
    janus_plugin* plugin,
    janus_plugin_session* janusSession,
    const std::string& transaction,
    const char* errorText);
