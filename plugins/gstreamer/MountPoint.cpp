#include "MountPoint.h"

#include <algorithm>
#include <cassert>

#include "GlibPtr.h"
#include "GstPtr.h"
#include "JsonPtr.h"
#include "Session.h"


enum {
    RECONNECT_TIMEOUT = 5,
};


bool operator == (const MountPoint::Client& client, janus_plugin_session* janusSession)
{
    return client.janusSessionPtr.get() == janusSession;
}

bool operator < (const MountPoint::ListinerAction& x, const MountPoint::ListinerAction& y)
{
    return x.janusSessionPtr.get() < y.janusSessionPtr.get();
}


MountPoint::MountPoint(janus_callbacks* janus, janus_plugin* plugin, const std::string& mrl) :
    _janus(janus), _plugin(plugin), _mrl(mrl), _prepared(false)
{
}

const std::string& MountPoint::mrl() const
{
    return _mrl;
}

void MountPoint::pushSdp(janus_plugin_session* janusSession, const std::string& transaction)
{
    Session* session = GetSession(janusSession);

    GstSDPMessage* outSdp;
    gst_sdp_message_new(&outSdp);
    GstSDPMessagePtr outSdpPtr(outSdp);

    gst_sdp_message_set_version(outSdp, "0");
    gst_sdp_message_set_origin(outSdp,
        "-", session->sdpSessionId.get(), "1", "IN", "IP4", "127.0.0.1");

    gst_sdp_message_set_session_name(outSdp, "Session streamed with Janus Gstreamer plugin");

    const GstSDPMessage* sourceSdp = media()->sdp();
    const guint mediaCount = gst_sdp_message_medias_len(sourceSdp);
    for(unsigned m = 0; m < mediaCount; ++m) {
        const GstSDPMedia* inMedia = gst_sdp_message_get_media(sourceSdp, m);
        GstSDPMedia* outMedia;
        gst_sdp_media_copy(inMedia, &outMedia);

        gst_sdp_media_set_port_info(outMedia, 1, 1); // Have to set port to some non zero value. Why?

        gst_sdp_message_add_media(outSdp, outMedia);
    }

    GCharPtr sdpPtr(gst_sdp_message_as_text(outSdp));
    const gchar* sdp = sdpPtr.get();
    JANUS_LOG(LOG_VERB, "PushSdp. \n%s\n", sdpPtr.get());

    JsonPtr eventPtr(json_object());
    json_t* event = eventPtr.get();

    json_object_set_new(event, "streaming", json_string("event"));

    JsonPtr jsepPtr(
        json_pack("{ssss}",
            "type", "offer",
            "sdp", sdp));
    json_t* jsep = jsepPtr.get();

    _janus->push_event(
        janusSession, _plugin,
        transaction.c_str(), event, jsep);
}

void MountPoint::mediaPrepared()
{
    _streams.resize(_media->streamsCount());
    _prepared = true;

    for(const Client& client: _clients)
        pushSdp(client.janusSessionPtr.get(), client.transaction);
}

void MountPoint::onBuffer(
    int stream,
    const void* data, gsize size)
{
    if(!_prepared)
        return;

    Stream& s = _streams[stream];
    if(s.actionsAvailable) {
        std::deque<ListinerAction> listinersActions;

        _modifyListenersGuard.lock();
        listinersActions.swap(s.listinersActions);
        s.actionsAvailable = false;
        _modifyListenersGuard.unlock();

        std::stable_sort(listinersActions.begin(), listinersActions.end());
        for(auto it = listinersActions.begin(); it != listinersActions.end(); ) {
            auto nextIt = it + 1;
            if(nextIt != listinersActions.end() &&
               it->janusSessionPtr == nextIt->janusSessionPtr &&
               it->add != nextIt->add)
            {
                it += 2;
            } else {
                janus_plugin_session* janusSession = it->janusSessionPtr.get();
                const auto listinerIt =
                    std::lower_bound(s.listiners.begin(), s.listiners.end(), janusSession);
                if(it->add) {
                    if(listinerIt == s.listiners.end() || *listinerIt != it->janusSessionPtr)
                        s.listiners.emplace(listinerIt, std::move(it->janusSessionPtr));
                } else {
                    if(listinerIt != s.listiners.end() && *listinerIt == janusSession)
                        s.listiners.erase(listinerIt);
                }
                ++it;
            }
        }
        assert(std::is_sorted(s.listiners.begin(), s.listiners.end()));
    }

    for(JanusPluginSessionPtr& janusSession: s.listiners) {
        _janus->relay_rtp(
            janusSession.get(), TRUE,
            (char*)data, size);
    }
}

void MountPoint::onEos(bool error)
{
    _media->shutdown();
    _media.reset();

    // FIXME! Should notify watcher about a problem?

    JANUS_LOG(LOG_INFO,
        "Scheduling reconnect to  \"%s\"\n",
        _mrl.c_str());
    auto reconnect =
         [] (gpointer userData) -> gboolean
    {
        MountPoint* mountPoint = static_cast<MountPoint*>(userData);
        // FIXME! take into account application shutdown
        if(!mountPoint->_clients.empty())
            mountPoint->prepareMedia();

        return FALSE;
    };

    GSourcePtr timeoutSourcePtr(g_timeout_source_new_seconds(RECONNECT_TIMEOUT));
    GSource* timeoutSource = timeoutSourcePtr.get();
    g_source_set_callback(
        timeoutSource,
        (GSourceFunc) reconnect,
        this, nullptr);
    g_source_attach(timeoutSource, g_main_context_get_thread_default());
}

void MountPoint::prepareMedia()
{
    if(_media)
        return;

    _media.reset(new Media(_mrl));
    _media->run(
        std::bind(&MountPoint::mediaPrepared, this),
        std::bind(&MountPoint::onBuffer, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
        std::bind(&MountPoint::onEos, this, std::placeholders::_1)
     );
}

const Media* MountPoint::media() const
{
    return _media.get();
}

void MountPoint::addWatcher(
    janus_plugin_session* janusSession,
    const std::string& transaction)
{
    janus_refcount_increase(&janusSession->ref);
    _clients.emplace_back(Client{JanusPluginSessionPtr(janusSession), transaction});

    if(media()->hasSdp())
         pushSdp(janusSession, transaction);
}

void MountPoint::startStream(janus_plugin_session* janusSession)
{
    std::lock_guard<std::mutex> lock(_modifyListenersGuard);
    for(Stream& s: _streams) {
        janus_refcount_increase(&janusSession->ref);
        s.listinersActions.emplace_back(
            ListinerAction{JanusPluginSessionPtr(janusSession), true});
        s.actionsAvailable = true;
    }
}

void MountPoint::stopStream(janus_plugin_session* janusSession)
{
    std::lock_guard<std::mutex> lock(_modifyListenersGuard);
    for(Stream& s: _streams) {
        janus_refcount_increase(&janusSession->ref);
        s.listinersActions.emplace_back(
            ListinerAction{JanusPluginSessionPtr(janusSession), false});
        s.actionsAvailable = true;
    }
}

void MountPoint::removeWatcher(janus_plugin_session* janusSession)
{
    _clients.erase(
        std::remove(_clients.begin(), _clients.end(), janusSession),
        _clients.end());

    if(_clients.empty()) {
        _media->shutdown();
        _media.reset();
        _streams.clear();
        _prepared = false;
    }
}
