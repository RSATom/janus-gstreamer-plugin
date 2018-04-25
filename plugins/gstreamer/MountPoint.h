#pragma once

#include <deque>
#include <mutex>

extern "C" {
#include "janus/plugins/plugin.h"
}

#include "JanusPtr.h"
#include "Media.h"


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
    friend bool operator < (const Client&, janus_plugin_session*);

    struct ListinerAction
    {
        JanusPluginSessionPtr janusSessionPtr;
        bool add;
    };
    friend bool operator < (const ListinerAction&, const ListinerAction&);

    struct Stream
    {
        std::deque<ListinerAction> listinersActions;
        bool actionsAvailable;

        std::deque<JanusPluginSessionPtr> listiners;
    };

    void pushError(const char* errorText);
    void pushError(
        janus_plugin_session* janusSession,
        const std::string& transaction,
        const char* errorText);
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
    unsigned _reconnectCount;
    std::deque<Stream> _streams;
    bool _prepared;

    std::mutex _modifyListenersGuard;
};
