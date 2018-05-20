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
    enum Flags {
        RESTREAM_VIDEO = 0x1,
        RESTREAM_AUDIO = 0x2,
        RESTREAM_BOTH = RESTREAM_VIDEO | RESTREAM_AUDIO,
    };

    MountPoint(
        janus_callbacks*, janus_plugin*, Flags,
        const std::string& description);

    const std::string& description() const;

    bool isUsed() const;

    void prepareMedia();

    void addWatcher(janus_plugin_session*, const std::string& transaction);
    void startStream(janus_plugin_session*, const std::string& transaction);
    void stopStream(janus_plugin_session*, const std::string& transaction);
    void removeWatcher(janus_plugin_session*);

protected:
    virtual std::unique_ptr<Media> createMedia() = 0;

private:
    struct Client
    {
        JanusPluginSessionPtr janusSessionPtr;
        std::string transaction;
    };
    friend bool operator == (const Client&, janus_plugin_session*);
    friend bool operator < (const Client&, janus_plugin_session*);
    friend bool operator != (const MountPoint::Client&, janus_plugin_session*);

    struct ListinerAction
    {
        JanusPluginSessionPtr janusSessionPtr;
        bool add;
    };
    friend bool operator < (const ListinerAction&, const ListinerAction&);

    enum class RestreamAs {
        None,
        Video,
        Audio,
    };

    struct Stream
    {
        RestreamAs restreamAs;

        std::deque<ListinerAction> listinersActions;
        bool actionsAvailable;

        std::deque<JanusPluginSessionPtr> listiners;
    };

    const std::string& mrl() const;

    const Media* media() const;

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

    const std::string _description;

    const Flags _flags;

    std::deque<Client> _clients;

    std::unique_ptr<Media> _media;
    unsigned _reconnectCount;
    std::deque<Stream> _streams;
    bool _prepared;

    std::mutex _modifyListenersGuard;
};
