#pragma once

#include <deque>
#include <mutex>

extern "C" {
#include "janus/plugins/plugin.h"
}

#include "JanusPtr.h"
#include "Media.h"


struct Stream
{
    std::deque<JanusPluginSessionPtr> addListiners;
    std::deque<janus_plugin_session*> removeListiners;

    bool modifyListiners;

    std::deque<JanusPluginSessionPtr> listiners;
};

class MountPoint
{
public:
    MountPoint(janus_callbacks*, janus_plugin*, const std::string& mrl);

    const std::string& mrl() const;

    void prepareMedia();
    const Media* media() const;

    void addWatcher(janus_plugin_session*, const std::string& transaction);
    void startStream(janus_plugin_session*);
    void stopStream(janus_plugin_session*);
    void removeWatcher(janus_plugin_session*);

private:
    struct Client
    {
        JanusPluginSessionPtr janusSessionPtr;
        std::string transaction;
    };
    friend bool operator == (const Client&, janus_plugin_session*);

    void pushSdp(janus_plugin_session*, const std::string& transaction);
    void mediaPrepared();
    void onBuffer(
        int stream,
        const void* data, gsize size);
    void onEos(bool error);

private:
    janus_callbacks *const _janus;
    janus_plugin *const _plugin;

    const std::string _mrl;

    std::deque<Client> _clients;

    std::unique_ptr<Media> _media;
    std::deque<Stream> _streams;
    bool _prepared;

    std::mutex _modifyListenersGuard;
};
