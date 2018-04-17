#include <memory>
#include <thread>
#include <deque>
#include <mutex>
#include <map>
#include <functional>
#include <algorithm>

#include <jansson.h>
#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>
#include <gst/app/gstappsink.h>


extern "C" {
#include "janus/plugins/plugin.h"
#include "janus/debug.h"
#include "janus/utils.h"
}

#include "JsonPtr.h"
#include "GlibPtr.h"
#include "GstPtr.h"
#include "QueueSource.h"
#include "Media.h"


namespace
{

enum {
    PLUGIN_VERSION = 1,
};

enum {
    INVALID_JSON_ERROR = 451,
};

enum class Request
{
    Invalid,
    List,
    Watch,
    Start,
    Stop,
};

struct Client
{
    janus_plugin_session* session;
    std::string transaction;
};

struct Stream
{
    std::deque<janus_plugin_session*> addListiners;
    std::deque<janus_plugin_session*> removeListiners;

    bool modifyListiners;

    std::deque<janus_plugin_session*> listiners;
};

struct MountPoint
{
    std::string mrl;

    std::deque<Client> watchers;

    std::unique_ptr<Media> source;

    std::deque<Stream> streams;

    bool prepared;
};

struct PluginContext
{
    janus_plugin janusPlugin;
    janus_callbacks* janus;

    GMainContextPtr mainContextPtr;
    GMainLoopPtr loopPtr;
    QueueSourcePtr queueSourcePtr;
    std::thread mainThread;

    std::mutex modifyListenersGuard;
    std::map<int, MountPoint> mountPoints;
};

struct Session
{
    MountPoint* watching;
    GCharPtr sdpSessionId;
};

struct PluginMessage : public QueueItem
{
    janus_plugin_session* handle;
    std::string transaction;
    JsonPtr json;
};

}


static const char* PluginStringVersion = "0.0.1";
static const char* PluginDescription   = "Gstreamer plugin";
static const char* PluginName          = "Gstreamer plugin";
static const char* PluginAuthor        = "Sergey Radionov";
static const char* PluginPackage       = "janus.plugin.gstreamer";

static PluginContext& Context();

static int GetApiCompatibility();
static int GetVersion();
static const char* GetVersionString();
static const char* GetDescription();
static const char* GetName();
static const char* GetAuthor();
static const char* GetPackage();

static int Init(janus_callbacks* callback, const char* configPath);
static void Destroy();
static void CreateSession(janus_plugin_session* handle, int* error);
static void DestroySession(janus_plugin_session* handle, int* error);
static struct janus_plugin_result*
HandleMessage(janus_plugin_session* handle, char* transaction, json_t* message, json_t* jsep);
static void SetupMedia(janus_plugin_session* handle);
static void HangupMedia(janus_plugin_session* handle);
static json_t* QuerySession(janus_plugin_session* handle);


extern "C" janus_plugin* create()
{
    JANUS_LOG(LOG_DBG, ">>>> %s: create\n", PluginName);

    return &Context().janusPlugin;
}

static int GetApiCompatibility()
{
    return JANUS_PLUGIN_API_VERSION;
}

static int GetVersion()
{
    return PLUGIN_VERSION;
}

static const char* GetVersionString()
{
    return PluginStringVersion;
}

static const char* GetDescription()
{
    return PluginDescription;
}

static const char* GetName(void)
{
    return PluginName;
}

static const char* GetAuthor(void)
{
    return PluginAuthor;
}

static const char* GetPackage(void)
{
    return PluginPackage;
}

static PluginContext& Context()
{
    static PluginContext context = {
        .janusPlugin = {
            .init                  = Init,
            .destroy               = Destroy,

            .get_api_compatibility = GetApiCompatibility,
            .get_version           = GetVersion,
            .get_version_string    = GetVersionString,
            .get_description       = GetDescription,
            .get_name              = GetName,
            .get_author            = GetAuthor,
            .get_package           = GetPackage,

            .create_session        = CreateSession,
            .handle_message        = HandleMessage,
            .setup_media           = SetupMedia,
            .incoming_rtp          = nullptr,
            .incoming_rtcp         = nullptr,
            .incoming_data         = nullptr,
            .slow_link             = nullptr,
            .hangup_media          = HangupMedia,
            .destroy_session       = DestroySession,
            .query_session         = QuerySession,
        }
    };

    return context;
}

static void PushSdp(const Client& watcher, const MountPoint& mountPoint)
{
    Session* session = static_cast<Session*>(watcher.session->plugin_handle);

    GstSDPMessage* outSdp;
    gst_sdp_message_new(&outSdp);
    GstSDPMessagePtr outSdpPtr(outSdp);

    gst_sdp_message_set_version(outSdp, "0");
    gst_sdp_message_set_origin(outSdp,
        "-", session->sdpSessionId.get(), "1", "IN", "IP4", "127.0.0.1");

    gst_sdp_message_set_session_name(outSdp, "Session streamed with Janus Gstreamer plugin");

    const GstSDPMessage* sourceSdp = mountPoint.source->sdp();
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

    Context().janus->push_event(
        watcher.session, &Context().janusPlugin,
        watcher.transaction.c_str(), event, jsep);
}

static void OnBuffer(
    MountPoint* mountPoint,
    int stream,
    const void* data, gsize size)
{
    PluginContext& context = Context();

    if(!mountPoint->prepared)
        return;

    Stream& s = mountPoint->streams[stream];
    if(s.modifyListiners) {
        std::deque<janus_plugin_session*> addListiners;
        std::deque<janus_plugin_session*> removeListiners;

        context.modifyListenersGuard.lock();
        addListiners.swap(s.addListiners);
        removeListiners.swap(s.removeListiners);
        s.modifyListiners = false;
        context.modifyListenersGuard.unlock();

        for(janus_plugin_session* session: removeListiners) {
            auto it =
                std::find(s.listiners.begin(), s.listiners.end(), session);
            if(it == s.listiners.end()) {
                g_warn_if_reached();
                continue;
            } else if(it == s.listiners.end() -1) {
                s.listiners.erase(it);
            } else {
                *it = *(s.listiners.end() -1);
                s.listiners.erase(s.listiners.end() -1);
            }
        }

        s.listiners.insert(
            s.listiners.end(),
            addListiners.begin(), addListiners.end());
    }

    for(janus_plugin_session* session: s.listiners) {
        context.janus->relay_rtp(
            session, TRUE,
            (char*)data, size);
    }
}

static void OnEos(MountPoint* mountPoint, bool error)
{
}

static void MediaPrepared(MountPoint* mountPoint)
{
    mountPoint->streams.resize(mountPoint->source->streamsCount());
    mountPoint->prepared = true;
    for(const Client& watcher: mountPoint->watchers)
        PushSdp(watcher, *mountPoint);
}

static void PrepareSource(MountPoint* mountPoint)
{
    if(mountPoint->source)
        return;

    PluginContext& context = Context();

    mountPoint->source.reset(new Media(mountPoint->mrl, context.loopPtr.get()));
    mountPoint->source->run(
        std::bind(MediaPrepared, mountPoint),
        std::bind(OnBuffer, mountPoint, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
        std::bind(OnEos, mountPoint, std::placeholders::_1)
     );
}

static Request ParseRequest(const json_t* message)
{
    const char* strRequest = nullptr;
    if(json_t* jsonRequest = json_object_get(message, "request"))
        strRequest = json_string_value(jsonRequest);

    if(!strRequest)
        return Request::Invalid;
    else if(0 == strcasecmp(strRequest, "list"))
        return Request::List;
    else if(0 == strcasecmp(strRequest, "watch"))
        return Request::Watch;
    else if(0 == strcasecmp(strRequest, "start"))
        return Request::Start;
    else if(0 == strcasecmp(strRequest, "stop"))
        return Request::Stop;
    else {
        JANUS_LOG(LOG_ERR, "%s: unsupported request \"%s\"\n", PluginName, strRequest);
        return Request::Invalid;
    }
}

static Request ParseRequest(JsonPtr& message)
{
    return ParseRequest(message.get());
}

static void HandleWatchMessage(
    janus_plugin_session* handle,
    const std::string& transaction,
    const JsonPtr& message)
{
    PluginContext& context = Context();

    Session* session = static_cast<Session*>(handle->plugin_handle);

    json_int_t id = -1;
    if(json_t* jsonId = json_object_get(message.get(), "id"))
        id = json_integer_value(jsonId);

    auto it = context.mountPoints.find(id);
    if(context.mountPoints.end() == it) {
        JANUS_LOG(LOG_ERR, "%s: unknown mount point id \"%lld\"\n", PluginName, id);
        return;
    }

    MountPoint& mountPoint = it->second;
    PrepareSource(&mountPoint);

    if(!mountPoint.source->hasSdp())
        mountPoint.watchers.push_back(Client{handle, transaction});
    else {
        Client client {handle, transaction};
        PushSdp(client, mountPoint);
    }

    session->sdpSessionId.reset(g_strdup_printf("%" PRId64, janus_get_real_time()));
    session->watching = &mountPoint;
}

static void HandleStartMessage(
    janus_plugin_session* handle,
    const std::string& transaction,
    const JsonPtr& message)
{
    PluginContext& context = Context();

    Session* session = static_cast<Session*>(handle->plugin_handle);

    if(!session->watching) {
        JANUS_LOG(LOG_ERR, "%s: trying to play without watch\n", PluginName);
        return;
    }

    MountPoint& mountPoint = *session->watching;
    std::lock_guard<std::mutex> lock(context.modifyListenersGuard);
    for(Stream& s: mountPoint.streams) {
        s.addListiners.push_back(handle);
        s.modifyListiners = true;
    }
}

static void HandleStopMessage(
    janus_plugin_session* handle,
    const std::string& transaction,
    const JsonPtr& message)
{
    PluginContext& context = Context();

    Session* session = static_cast<Session*>(handle->plugin_handle);

    if(!session->watching) {
        JANUS_LOG(LOG_ERR, "%s: trying to stop without watch\n", PluginName);
        return;
    }

    MountPoint& mountPoint = *session->watching;
    std::lock_guard<std::mutex> lock(context.modifyListenersGuard);
    for(Stream& s: mountPoint.streams) {
        s.removeListiners.push_back(handle);
        s.modifyListiners = true;
    }
}

static void HandlePluginMessage(const std::unique_ptr<QueueItem>& item, gpointer /*userData*/)
{
    PluginMessage& message = static_cast<PluginMessage&>(*item);

    const Request request = ParseRequest(message.json);
    switch(request) {
    case Request::Watch:
        JANUS_LOG(LOG_DBG, "%s: HandlePluginMessage. Request::Watch\n", PluginName);
        HandleWatchMessage(message.handle, message.transaction, message.json);
        break;
    case Request::Start:
        JANUS_LOG(LOG_DBG, "%s: HandlePluginMessage. Request::Start\n", PluginName);
        HandleStartMessage(message.handle, message.transaction, message.json);
        break;
    case Request::Stop:
        JANUS_LOG(LOG_DBG, "%s: HandlePluginMessage. Request::Stop\n", PluginName);
        HandleStopMessage(message.handle, message.transaction, message.json);
        break;
    case Request::Invalid:
        JANUS_LOG(LOG_DBG, "%s: HandlePluginMessage. Request::Invalid\n", PluginName);
        break;
    case Request::List:
        JANUS_LOG(LOG_ERR, "%s: HandlePluginMessage. Request::List is unexpected\n", PluginName);
        break;
    default:
        JANUS_LOG(LOG_ERR, "%s: HandlePluginMessage. Unknown request\n", PluginName);
        break;
    }
}

static void PostPluginMessage(
    janus_plugin_session* handle,
    char* transaction,
    json_t* message)
{
    std::unique_ptr<PluginMessage> pluginMessagePtr = std::make_unique<PluginMessage>();
    pluginMessagePtr->handle = handle;
    pluginMessagePtr->transaction = transaction;
    pluginMessagePtr->json.reset(json_incref(message));

    QueueSourcePush(
        Context().queueSourcePtr,
        pluginMessagePtr.release());
}


static void PluginMain()
{
    JANUS_LOG(LOG_DBG, ">>>> %s: GstreamerMain\n", PluginName);

    gst_init(0, nullptr);

    PluginContext& context = Context();

    context.mainContextPtr.reset(g_main_context_new());
    GMainContext* mainContext = context.mainContextPtr.get();
    context.loopPtr.reset(g_main_loop_new(mainContext, FALSE));
    context.queueSourcePtr = QueueSourceNew(mainContext, HandlePluginMessage, nullptr);

    GMainLoop* loop = context.loopPtr.get();

    g_main_loop_run(loop);

    context.loopPtr.reset();
    context.mainContextPtr.reset();

    gst_deinit();
}

static void InitPluginMain()
{
    PluginContext& context = Context();

    context.mainThread = std::thread(PluginMain);
}

static int Init(janus_callbacks* callback, const char* configPath)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: init\n", PluginName);

    PluginContext& context = Context();

    context.janus = callback;

    InitPluginMain();

    context.mountPoints.emplace(1,
        MountPoint{
            "rtsp://restream-basic.eastasia.cloudapp.azure.com:8090/bars",
        });
    context.mountPoints.emplace(2,
        MountPoint{
            "rtsp://restream-basic.eastasia.cloudapp.azure.com:8100/bars",
        });
    context.mountPoints.emplace(3,
        MountPoint{
            "rtsp://restream-basic.eastasia.cloudapp.azure.com:8100/dlink931",
        });

    return 0;
}

static void Destroy()
{
    JANUS_LOG(LOG_DBG, ">>>> %s: destroy\n", PluginName);

    Context().mainThread.join();
}

static void CreateSession(janus_plugin_session* handle, int* error)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: create_session\n", PluginName);

    std::unique_ptr<Session> sessionPtr = std::make_unique<Session>();

    handle->plugin_handle = sessionPtr.release();
}

static void DestroySession(janus_plugin_session* handle, int* error)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: destroy_session\n", PluginName);

    std::unique_ptr<Session> sessionPtr(
        static_cast<Session*>(handle->plugin_handle));

    handle->plugin_handle = nullptr;
}

static janus_plugin_result* InvalidJson(const char* errorText)
{
    JANUS_LOG(LOG_ERR, "%s", errorText);

    return
        janus_plugin_result_new(
            static_cast<janus_plugin_result_type>(INVALID_JSON_ERROR), errorText, nullptr);
}

static struct janus_plugin_result* HandleList()
{
    PluginContext& context = Context();

    JsonPtr listPtr(json_array());
    json_t* list = listPtr.get();

    for(auto& pair: context.mountPoints) {
        JsonPtr listItemPtr(json_object());
        json_t* listItem = listItemPtr.get();

        json_object_set_new(listItem, "id", json_integer(pair.first));
        json_object_set_new(listItem, "description", json_string(pair.second.mrl.c_str()));
        json_object_set_new(listItem, "type", json_string("live"));
        json_array_append_new(list, listItemPtr.release());
    }

    JsonPtr responsePtr(json_object());
    json_t* response = responsePtr.get();

    json_object_set_new(response, "streaming", json_string("list"));
    json_object_set_new(response, "list", listPtr.release());

    return
        janus_plugin_result_new(
            JANUS_PLUGIN_OK, nullptr, responsePtr.release());
}

struct janus_plugin_result* HandleMessage(
    janus_plugin_session* handle,
    char* transaction,
    json_t* message,
    json_t* jsep)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: handle_message\n", PluginName);

    {
        char* json = json_dumps(message, JSON_INDENT(4));
        JANUS_LOG(LOG_DBG, "message:\n%s\n", json);
        free(json);
    }

    if(jsep) {
        char* json = json_dumps(jsep, JSON_INDENT(4));
        JANUS_LOG(LOG_DBG, "jsep:\n%s\n", json);
        free(json);
    }

    if(!json_is_object(message))
        return InvalidJson("JSON error: not an object\n");

    const Request request = ParseRequest(message);
    switch(request) {
    case Request::Invalid:
        JANUS_LOG(LOG_ERR, "%s: Request::Invalid\n", PluginName);
        return InvalidJson("JSON error: invalid request\n");
    case Request::List:
        JANUS_LOG(LOG_DBG, "%s: Request::List\n", PluginName);
        return HandleList();
    case Request::Watch:
        JANUS_LOG(LOG_DBG, "%s: Request::Watch\n", PluginName);

        PostPluginMessage(handle, transaction, message);

        return
            janus_plugin_result_new(
                JANUS_PLUGIN_OK_WAIT, nullptr, nullptr);
    case Request::Start: {
        JANUS_LOG(LOG_DBG, "%s: Request::Start\n", PluginName);

        PostPluginMessage(handle, transaction, message);

        return
            janus_plugin_result_new(
                JANUS_PLUGIN_OK_WAIT, nullptr, nullptr);
    }
    case Request::Stop: {
        JANUS_LOG(LOG_DBG, "%s: Request::Stop\n", PluginName);

        PostPluginMessage(handle, transaction, message);

        return
            janus_plugin_result_new(
                JANUS_PLUGIN_OK_WAIT, nullptr, nullptr);
        break;
    }
    default:
        return InvalidJson("JSON error: unknown request\n");
    }

    Session* session = static_cast<Session*>(handle->plugin_handle);

    return janus_plugin_result_new(JANUS_PLUGIN_ERROR, "Internal error", NULL);
}

void SetupMedia(janus_plugin_session* handle)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: setup_media\n", PluginName);
}

void HangupMedia(janus_plugin_session* handle)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: hangup_media\n", PluginName);
}

json_t* QuerySession(janus_plugin_session* handle)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: query_session\n", PluginName);

    return NULL;
}
