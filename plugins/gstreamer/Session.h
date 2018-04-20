#pragma once

#include "GlibPtr.h"

#include "MountPoint.h"


struct Session
{
    MountPoint* watching;
    GCharPtr sdpSessionId;
};

inline Session* GetSession(janus_plugin_session* janusSession)
    { return static_cast<Session*>(janusSession->plugin_handle); }
