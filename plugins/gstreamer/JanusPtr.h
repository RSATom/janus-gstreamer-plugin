#pragma once

#include <memory>


struct JanusUnref
{
    void operator() (janus_plugin_session* janusSession)
        { janus_refcount_decrease(&janusSession->ref); }
};

typedef
    std::unique_ptr<
        janus_plugin_session,
        JanusUnref> JanusPluginSessionPtr;

inline bool operator == (const JanusPluginSessionPtr& x, const janus_plugin_session* y)
    { return x.get() == y; }
