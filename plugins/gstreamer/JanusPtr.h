#pragma once

#include <memory>

extern "C" {
#include "janus/config.h"
}

struct JanusUnref
{
    void operator() (janus_plugin_session* janusSession)
        { janus_refcount_decrease(&janusSession->ref); }

    void operator() (janus_config* config)
        { janus_config_destroy(config); }
};

typedef
    std::unique_ptr<
        janus_plugin_session,
        JanusUnref> JanusPluginSessionPtr;

inline bool operator == (const JanusPluginSessionPtr& x, const janus_plugin_session* y)
    { return x.get() == y; }
inline bool operator < (const JanusPluginSessionPtr& x, const janus_plugin_session* y)
    { return x.get() < y; }

typedef
    std::unique_ptr<
        janus_config,
        JanusUnref> JanusConfigPtr;
