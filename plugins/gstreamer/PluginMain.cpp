#include "PluginMain.h"

#include <cassert>

extern "C" {
#include "janus/debug.h"
#include "janus/utils.h"
}

#include "JsonPtr.h"
#include "PluginContext.h"
#include "Session.h"
#include "Request.h"


namespace {

// FIXME! add ability send different meessage structs
struct PluginMessage : public QueueItem
{
    JanusPluginSessionPtr janusSessionPtr;
    std::string transaction;
    JsonPtr json;
    bool destroy;
};

}

static void StopWatching(janus_plugin_session* janusSession)
{
    Session* session = GetSession(janusSession);

    if(session->watching) {
        session->watching->removeWatcher(janusSession);
        session->watching = nullptr;
        session->sdpSessionId.reset();
    }
}

static void HandleWatchMessage(
    janus_plugin_session* janusSession,
    const std::string& transaction,
    const JsonPtr& message)
{
    PluginContext& context = Context();

    Session* session = GetSession(janusSession);
    if(session->watching) {
        JANUS_LOG(LOG_INFO,
            "%s: already watching \"%s\". Detaching...\n",
            GetPluginName(),
            session->watching->description().c_str());

        StopWatching(janusSession);
    }

    json_int_t id = -1;
    if(json_t* jsonId = json_object_get(message.get(), "id"))
        id = json_integer_value(jsonId);

    auto it = context.mountPoints.find(id);
    if(context.mountPoints.end() == it) {
        JANUS_LOG(LOG_ERR, "%s: unknown mount point id \"%lld\"\n", GetPluginName(), id);
        return;
    }

    assert(!session->sdpSessionId);
    if(!session->sdpSessionId)
        session->sdpSessionId.reset(g_strdup_printf("%" PRId64, janus_get_real_time()));

    MountPoint* mountPoint = it->second.get();

    mountPoint->addWatcher(janusSession, transaction);

    session->watching = mountPoint;

    mountPoint->prepareMedia();
}

static void HandleStartMessage(
    janus_plugin_session* janusSession,
    const std::string& transaction,
    const JsonPtr& message)
{
    PluginContext& context = Context();

    Session* session = GetSession(janusSession);

    if(!session->watching) {
        JANUS_LOG(LOG_ERR, "%s: trying to play without watch\n", GetPluginName());
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
        JANUS_LOG(LOG_ERR, "%s: trying to stop without watch\n", GetPluginName());
        return;
    }

    session->watching->stopStream(janusSession, transaction);
}

static void HandleDestroyMessage(janus_plugin_session* janusSession)
{
    Session* session = GetSession(janusSession);
    std::unique_ptr<Session> SessionPtr(session);

    StopWatching(janusSession);

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
        JANUS_LOG(LOG_DBG, "%s: HandlePluginMessage. Request::Watch\n", GetPluginName());
        HandleWatchMessage(message.janusSessionPtr.get(), message.transaction, message.json);
        break;
    case Request::Start:
        JANUS_LOG(LOG_DBG, "%s: HandlePluginMessage. Request::Start\n", GetPluginName());
        HandleStartMessage(message.janusSessionPtr.get(), message.transaction, message.json);
        break;
    case Request::Stop:
        JANUS_LOG(LOG_DBG, "%s: HandlePluginMessage. Request::Stop\n", GetPluginName());
        HandleStopMessage(message.janusSessionPtr.get(), message.transaction, message.json);
        break;
    case Request::Invalid:
        JANUS_LOG(LOG_DBG, "%s: HandlePluginMessage. Request::Invalid\n", GetPluginName());
        break;
    case Request::List:
        JANUS_LOG(LOG_ERR, "%s: HandlePluginMessage. Request::List is unexpected\n", GetPluginName());
        break;
    default:
        JANUS_LOG(LOG_ERR, "%s: HandlePluginMessage. Unknown request\n", GetPluginName());
        break;
    }
}

static void PluginMain()
{
    PluginContext& context = Context();

    JANUS_LOG(LOG_DBG, ">>>> %s: GstreamerMain\n", context.janusPlugin->get_name());

    gst_init(0, nullptr);

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

void StartPluginThread()
{
    PluginContext& context = Context();

    context.mainThread = std::thread(PluginMain);
}

void PostPluginMessage(
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
