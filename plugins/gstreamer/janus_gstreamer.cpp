#include <memory>
#include <thread>
#include <map>
#include <cassert>

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
#include "Session.h"
#include "QueueSource.h"
#include "MountPoint.h"
#include "ConfigLoader.h"


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

struct PluginContext
{
    janus_plugin janusPlugin;
    janus_callbacks* janus;

    GMainContextPtr mainContextPtr;
    GMainLoopPtr loopPtr;
    QueueSourcePtr queueSourcePtr;
    std::thread mainThread;

    std::map<int, MountPoint> mountPoints;
};

// FIXME! add ability send different meessage structs
struct PluginMessage : public QueueItem
{
    JanusPluginSessionPtr janusSessionPtr;
    std::string transaction;
    JsonPtr json;
    bool destroy;
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
static void CreateSession(janus_plugin_session*, int* error);
static void DestroySession(janus_plugin_session*, int* error);
static struct janus_plugin_result*
HandleMessage(
    janus_plugin_session*, char* transaction,
    json_t* message, json_t* jsep);
static void SetupMedia(janus_plugin_session*);
static void HangupMedia(janus_plugin_session*);
static json_t* QuerySession(janus_plugin_session*);


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

static Request ParseRequest(const json_t* message)
{
    const char* strRequest = nullptr;
    if(json_t* jsonRequest = json_object_get(message, "request"))
        strRequest = json_string_value(jsonRequest);

    if(!strRequest) {
        JANUS_LOG(LOG_ERR, "%s: no request\n", PluginName);
        return Request::Invalid;
    } else if(0 == strcasecmp(strRequest, "list"))
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
    janus_plugin_session* janusSession,
    const std::string& transaction,
    const JsonPtr& message)
{
    PluginContext& context = Context();

    Session* session = GetSession(janusSession);
    if(session->watching) {
        JANUS_LOG(LOG_ERR, "%s: already watching\n", PluginName);
        return;
    }

    json_int_t id = -1;
    if(json_t* jsonId = json_object_get(message.get(), "id"))
        id = json_integer_value(jsonId);

    auto it = context.mountPoints.find(id);
    if(context.mountPoints.end() == it) {
        JANUS_LOG(LOG_ERR, "%s: unknown mount point id \"%lld\"\n", PluginName, id);
        return;
    }

    assert(!session->sdpSessionId);
    if(!session->sdpSessionId)
        session->sdpSessionId.reset(g_strdup_printf("%" PRId64, janus_get_real_time()));

    MountPoint& mountPoint = it->second;
    mountPoint.prepareMedia();

    mountPoint.addWatcher(janusSession, transaction);

    session->watching = &mountPoint;
}

static void HandleStartMessage(
    janus_plugin_session* janusSession,
    const std::string& transaction,
    const JsonPtr& message)
{
    PluginContext& context = Context();

    Session* session = GetSession(janusSession);

    if(!session->watching) {
        JANUS_LOG(LOG_ERR, "%s: trying to play without watch\n", PluginName);
        return;
    }

    session->watching->startStream(janusSession, transaction);
}

static void HandleStopMessage(
    janus_plugin_session* janusSession,
    const std::string& transaction,
    const JsonPtr& message)
{
    PluginContext& context = Context();

    Session* session = GetSession(janusSession);

    if(!session->watching) {
        JANUS_LOG(LOG_ERR, "%s: trying to stop without watch\n", PluginName);
        return;
    }

    session->watching->stopStream(janusSession, transaction);
    session->watching = nullptr;
    session->sdpSessionId.reset();
}

static void HandleDestroyMessage(janus_plugin_session* janusSession)
{
    Session* session = GetSession(janusSession);
    std::unique_ptr<Session> SessionPtr(session);

    if(session->watching) {
        session->watching->removeWatcher(janusSession);
        session->watching = nullptr;
    }

    janusSession->plugin_handle = nullptr;
}

static void HandlePluginMessage(const std::unique_ptr<QueueItem>& item, gpointer /*userData*/)
{
    PluginMessage& message = static_cast<PluginMessage&>(*item);

    if(message.janusSessionPtr && message.destroy) {
        HandleDestroyMessage(message.janusSessionPtr.get());
        return;
    }

    const Request request = ParseRequest(message.json);
    switch(request) {
    case Request::Watch:
        JANUS_LOG(LOG_DBG, "%s: HandlePluginMessage. Request::Watch\n", PluginName);
        HandleWatchMessage(message.janusSessionPtr.get(), message.transaction, message.json);
        break;
    case Request::Start:
        JANUS_LOG(LOG_DBG, "%s: HandlePluginMessage. Request::Start\n", PluginName);
        HandleStartMessage(message.janusSessionPtr.get(), message.transaction, message.json);
        break;
    case Request::Stop:
        JANUS_LOG(LOG_DBG, "%s: HandlePluginMessage. Request::Stop\n", PluginName);
        HandleStopMessage(message.janusSessionPtr.get(), message.transaction, message.json);
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
    janus_plugin_session* janusSession,
    char* transaction,
    json_t* message,
    bool destroy)
{
    std::unique_ptr<PluginMessage> pluginMessagePtr = std::make_unique<PluginMessage>();
    janus_refcount_increase(&janusSession->ref);
    pluginMessagePtr->janusSessionPtr.reset(janusSession);
    if(transaction)
        pluginMessagePtr->transaction = transaction;
    if(message)
        pluginMessagePtr->json.reset(json_incref(message));
    pluginMessagePtr->destroy = destroy;

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

    g_main_context_push_thread_default(mainContext);

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

    LoadConfig(
        context.janus, &context.janusPlugin,
        std::string(configPath) + "/" + PluginPackage + ".jcfg",
        &context.mountPoints);

    InitPluginMain();

    return 0;
}

static void Destroy()
{
    JANUS_LOG(LOG_DBG, ">>>> %s: destroy\n", PluginName);

    Context().mainThread.join();
}

static void CreateSession(janus_plugin_session* janusSession, int* error)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: create_session\n", PluginName);

    std::unique_ptr<Session> sessionPtr = std::make_unique<Session>();

    janusSession->plugin_handle = sessionPtr.release();
}

static void DestroySession(janus_plugin_session* janusSession, int* error)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: destroy_session\n", PluginName);

    PostPluginMessage(janusSession, nullptr, nullptr, true);
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
        json_object_set_new(listItem, "description", json_string(pair.second.description().c_str()));
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
    janus_plugin_session* janusSession,
    char* transaction,
    json_t* message,
    json_t* jsep)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: HandleMessage\n", PluginName);

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

        PostPluginMessage(janusSession, transaction, message, false);

        return
            janus_plugin_result_new(
                JANUS_PLUGIN_OK_WAIT, nullptr, nullptr);
    case Request::Start: {
        JANUS_LOG(LOG_DBG, "%s: Request::Start\n", PluginName);

        PostPluginMessage(janusSession, transaction, message, false);

        return
            janus_plugin_result_new(
                JANUS_PLUGIN_OK_WAIT, nullptr, nullptr);
    }
    case Request::Stop: {
        JANUS_LOG(LOG_DBG, "%s: Request::Stop\n", PluginName);

        PostPluginMessage(janusSession, transaction, message, false);

        return
            janus_plugin_result_new(
                JANUS_PLUGIN_OK_WAIT, nullptr, nullptr);
        break;
    }
    default:
        return InvalidJson("JSON error: unknown request\n");
    }

    return janus_plugin_result_new(JANUS_PLUGIN_ERROR, "Internal error", NULL);
}

void SetupMedia(janus_plugin_session* /*janusSession*/)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: setup_media\n", PluginName);
}

void HangupMedia(janus_plugin_session* /*janusSession*/)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: hangup_media\n", PluginName);
}

json_t* QuerySession(janus_plugin_session* /*janusSession*/)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: query_session\n", PluginName);

    return NULL;
}
