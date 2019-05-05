#include "MountPoint.h"

#include <algorithm>
#include <cassert>

#include "CxxPtr/GlibPtr.h"
#include "CxxPtr/GstPtr.h"
#include "JanssonPtr.h"
#include "Session.h"


enum {
    RECONNECT_TIMEOUT = 5,
    MAX_RECONNECT_COUNT = 5,
    MAX_CLIENTS_COUNT = -1,
};


bool operator == (const MountPoint::Client& client, janus_plugin_session* janusSession)
{
    return client.janusSessionPtr.get() == janusSession;
}

bool operator < (const MountPoint::Client& client, janus_plugin_session* janusSession)
{
    return client.janusSessionPtr.get() < janusSession;
}

bool operator != (const MountPoint::Client& client, janus_plugin_session* janusSession)
{
    return client.janusSessionPtr.get() != janusSession;
}


bool operator < (const MountPoint::ListinerAction& x, const MountPoint::ListinerAction& y)
{
    return x.janusSessionPtr.get() < y.janusSessionPtr.get();
}


MountPoint::MountPoint(
    janus_callbacks* janus, janus_plugin* plugin,
    Flags flags, const std::string& description) :
    _janus(janus), _plugin(plugin),
    _flags(flags), _description(description),
    _reconnectCount(0), _prepared(false)
{
}

const std::string&  MountPoint::description() const
{
    return _description;
}

bool MountPoint::isUsed() const
{
    return !_clients.empty();
}

void MountPoint::pushError(const char* errorText)
{
    JANUS_LOG(LOG_ERR, "%s\n", errorText);

    JsonPtr eventPtr(json_object());
    json_t* event = eventPtr.get();

    json_object_set_new(event, "error", json_string(errorText));

    for(const Client& client: _clients) {
        _janus->push_event(
            client.janusSessionPtr.get(), _plugin,
            client.transaction.c_str(), event, nullptr);
    }
}

void MountPoint::pushError(
    janus_plugin_session* janusSession,
    const std::string& transaction,
    const char* errorText)
{
    PushError(_janus, _plugin, janusSession, transaction, errorText);
}

void MountPoint::pushSdp(janus_plugin_session* janusSession, const std::string& transaction)
{
    if(!media() || !media()->hasSdp()) {
        JANUS_LOG(LOG_ERR, "MountPoint::pushSdp. SDP missing.");
        return;
    }

    const bool restreamVideo = _flags & RESTREAM_VIDEO;
    const bool restreamAudio = _flags & RESTREAM_AUDIO;

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
        if(0 == g_strcmp0(inMedia->media, "video")) {
            if(!restreamVideo)
                continue;
        } else if(0 == g_strcmp0(inMedia->media, "audio")) {
            if(!restreamAudio)
                continue;
        } else
            continue;

        JANUS_LOG(LOG_VERB, "inMedia: %s\n", gst_sdp_media_as_text(inMedia));

        GstSDPMediaPtr outMediaPtr;

        if(0 == g_strcmp0(inMedia->media, "video")) {
            GstSDPMedia* outMedia;
            gst_sdp_media_new(&outMedia);
            outMediaPtr.reset(outMedia);

            gst_sdp_media_set_proto(outMedia, gst_sdp_media_get_proto(inMedia));

            const guint fmtCount = gst_sdp_media_formats_len(inMedia);
            for(guint fmtIdx = 0; fmtIdx < fmtCount; ++fmtIdx) {
                const gchar* fmt = gst_sdp_media_get_format(inMedia, fmtIdx);
                GstCaps* caps = gst_sdp_media_get_caps_from_media(inMedia, atoi(fmt));
                GstCapsPtr capsPtr(caps);
                if(caps) {
                    const guint size = gst_caps_get_size(caps);
                    for(int i = 0; i < size; ++i) {
                        GstStructure* structure = gst_caps_get_structure(caps, i);
                        const gchar* encodingName =
                            gst_structure_get_string(structure, "encoding-name");
                        if(0 == g_strcmp0(encodingName, "H264")) {
                            gst_structure_set(
                                structure,
                                "profile-level-id", G_TYPE_STRING, "42c015",
                                NULL);
                        }
                    }
                    gst_sdp_media_set_media_from_caps(caps, outMedia);
                }
            }
        } else {
            GstSDPMedia* outMedia;
            gst_sdp_media_copy(inMedia, &outMedia);
            outMediaPtr.reset(outMedia);
        }

        if(outMediaPtr) {
            gst_sdp_media_set_port_info(outMediaPtr.get(), 1, 1); // Have to set port to some non zero value. Why?
            gst_sdp_message_add_media(outSdp, outMediaPtr.get());
        }
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
    const bool restreamVideo = _flags & RESTREAM_VIDEO;
    const bool restreamAudio = _flags & RESTREAM_AUDIO;

    const std::vector<Media::Stream> streams = _media->streams();

    _streams.resize(streams.size());

    bool videoFound = false, audioFound = false;
    for(unsigned i = 0; i < streams.size(); ++i) {
        const Media::Stream& stream = streams[i];
        if(restreamVideo && !videoFound && Media::StreamType::Video == stream.type) {
            _streams[i].restreamAs = RestreamAs::Video;
            videoFound = true;
        } else if(restreamAudio && !audioFound && Media::StreamType::Audio == stream.type) {
            _streams[i].restreamAs = RestreamAs::Audio;
            audioFound = true;
        } else {
            _streams[i].restreamAs = RestreamAs::None;
        }
    }

    _prepared = true; // FIXME! protect from reordering

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

    if(RestreamAs::None == s.restreamAs)
        return;

    for(JanusPluginSessionPtr& janusSession: s.listiners) {
        _janus->relay_rtp(
            janusSession.get(), RestreamAs::Video == s.restreamAs ? TRUE : FALSE,
            (char*)data, size);
    }
}

void MountPoint::onEos(bool error)
{
    _media->shutdown();
    _media.reset();

    if(_reconnectCount >= MAX_RECONNECT_COUNT - 1) {
        JANUS_LOG(LOG_ERR,
            "Max reconnect count is reached\n");

        pushError("fail to start streaming");

        _reconnectCount = 0;

        return;
    }

    ++_reconnectCount;

    JANUS_LOG(LOG_INFO,
        "Scheduling reconnect to  \"%s\"\n",
        description().c_str());
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

    _media = createMedia();
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
    if(MAX_CLIENTS_COUNT >= 0 && _clients.size() >= MAX_CLIENTS_COUNT) {
        pushError(janusSession, transaction, "max clients count reached");
        return;
    }

    const auto clientIt =
        std::lower_bound(_clients.begin(), _clients.end(), janusSession);
    if(clientIt == _clients.end() || clientIt->janusSessionPtr.get() != janusSession) {
        janus_refcount_increase(&janusSession->ref);
        _clients.emplace(clientIt, Client{JanusPluginSessionPtr(janusSession), transaction});
    } else {
        JANUS_LOG(LOG_ERR, "janus session already watching\n");
        return;
    }

    if(media() && media()->hasSdp())
         pushSdp(janusSession, transaction);
}

void MountPoint::startStream(
    janus_plugin_session* janusSession,
    const std::string& transaction)
{
    const auto clientIt =
        std::lower_bound(_clients.begin(), _clients.end(), janusSession);
    if(clientIt == _clients.end() || *clientIt != janusSession) {
        pushError(janusSession, transaction, "start without attach");
        return;
    }

    std::lock_guard<std::mutex> lock(_modifyListenersGuard);
    for(Stream& s: _streams) {
        if(RestreamAs::None == s.restreamAs)
            continue;

        janus_refcount_increase(&janusSession->ref);
        s.listinersActions.emplace_back(
            ListinerAction{JanusPluginSessionPtr(janusSession), true});
        s.actionsAvailable = true;
    }
}

void MountPoint::stopStream(
    janus_plugin_session* janusSession,
    const std::string& transaction)
{
    const auto clientIt =
        std::lower_bound(_clients.begin(), _clients.end(), janusSession);
    if(clientIt == _clients.end() || *clientIt != janusSession) {
        pushError(janusSession, transaction, "stop without attach");
        return;
    }

    std::lock_guard<std::mutex> lock(_modifyListenersGuard);
    for(Stream& s: _streams) {
        if(RestreamAs::None == s.restreamAs)
            continue;

        janus_refcount_increase(&janusSession->ref);
        s.listinersActions.emplace_back(
            ListinerAction{JanusPluginSessionPtr(janusSession), false});
        s.actionsAvailable = true;
    }
}

void MountPoint::removeWatcher(janus_plugin_session* janusSession)
{
    const auto clientIt =
        std::lower_bound(_clients.begin(), _clients.end(), janusSession);
    if(clientIt != _clients.end() && *clientIt == janusSession)
        _clients.erase(clientIt);
    else
        JANUS_LOG(LOG_ERR, "trying to remove not watching session\n");

    if(_clients.empty()) {
        if(_media) {
            _media->shutdown();
            _media.reset();
            _streams.clear();
            _prepared = false;
        }
        assert(_streams.empty() && !_prepared);
    }
}
