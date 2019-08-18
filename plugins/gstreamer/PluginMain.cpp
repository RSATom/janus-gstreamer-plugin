#include "PluginMain.h"

#include <cassert>

extern "C" {
#include "janus/debug.h"
#include "janus/utils.h"
}

#include "CxxPtr/JanssonPtr.h"

#include "PluginContext.h"
#include "Session.h"
#include "Request.h"
#include "RtspMountPoint.h"


namespace {

struct PluginMessage : public QueueItem
{
    enum class Origin
    {
        Janus,
        Client,
    } origin;

    JanusPluginSessionPtr janusSessionPtr;
};

struct ClientMessage : public PluginMessage
{
    std::string transaction;
    JsonPtr json;
};

struct JanusMessage : public PluginMessage
{
    enum class Type
    {
        Hangup,
        Destroy,
    } type;
};

}

static void StopWatching(janus_plugin_session* janusSession)
{
    Session* session = GetSession(janusSession);

    if(session->watching) {
        session->watching->stopStream(janusSession);

        session->watching->removeWatcher(janusSession);

        if(session->dynamicMountPointWatching && !session->watching->isUsed())
            Context().dynamicMountPoints.erase(session->watching->description());

        session->watching = nullptr;
        session->sdpSessionId.reset();

        Context().janus->close_pc(janusSession);
    }
}

static void HandleWatchMessage(
    janus_plugin_session* janusSession,
    const std::string& transaction,
    const JsonPtr& message)
{
    PluginContext& context = Context();

    MountPoint* mountPoint = nullptr;

    json_int_t id = -1;
    if(json_t* jsonId = json_object_get(message.get(), "id")) {
        id = json_integer_value(jsonId);

        auto it = context.mountPoints.find(id);
        if(context.mountPoints.end() != it) {
            mountPoint = it->second.get();
        } else {
            JANUS_LOG(LOG_ERR, "%s: unknown mount point id \"%lld\"\n", GetPluginName(), id);
            PushError(
                context.janus,
                context.janusPlugin.get(),
                janusSession,
                transaction,
                "unknown mount point id");

            return;
        }
    }

    std::string mrl;
    if(id < 0 && context.config.enableDynamicMountPoints) {
        if(json_t* jsonMrl = json_object_get(message.get(), "mrl")) {
            mrl = json_string_value(jsonMrl);
            if(mrl.empty()) {
                JANUS_LOG(LOG_ERR, "%s: empty mrl\n", GetPluginName());
                PushError(
                    context.janus,
                    context.janusPlugin.get(),
                    janusSession,
                    transaction,
                    "empty mrl");

                return;
            }
        }
    }

    if(id < 0 && mrl.empty()) {
        if(context.config.enableDynamicMountPoints) {
            JANUS_LOG(LOG_ERR, "%s: missing mount point id and mrl\n", GetPluginName());
            PushError(
                context.janus,
                context.janusPlugin.get(),
                janusSession,
                transaction,
                "missing mount point id and mrl");
        } else {
            JANUS_LOG(LOG_ERR, "%s: missing mount point id\n", GetPluginName());
            PushError(
                context.janus,
                context.janusPlugin.get(),
                janusSession,
                transaction,
                "missing mount point id");
        }

        return;
    }

    Session* session = GetSession(janusSession);
    if(session->watching) {
        if((id >= 0 && session->watching != mountPoint) ||
           (context.config.enableDynamicMountPoints && session->watching->description() != mrl))
        {
            JANUS_LOG(LOG_ERR,
                "%s: already watching \"%s\".\n",
                GetPluginName(),
                session->watching->description().c_str());
            PushError(
                context.janus,
                context.janusPlugin.get(),
                janusSession,
                transaction,
                "already watching");
        }

        return;
    }

    if(id >= 0) {
        assert(mountPoint);
        session->dynamicMountPointWatching = false;
    } else if(context.config.enableDynamicMountPoints) {
        assert(!mrl.empty());
        session->dynamicMountPointWatching = true;

        auto it = context.dynamicMountPoints.find(mrl);
        if(context.dynamicMountPoints.end() == it) {
            if(context.dynamicMountPoints.size() < context.config.maxDynamicMountPoints) {
                it =
                    context.dynamicMountPoints.emplace(
                        std::piecewise_construct,
                        std::make_tuple(mrl),
                        std::make_tuple(
                            new RtspMountPoint(
                                context.janus, context.janusPlugin.get(),
                                mrl,
                                MountPoint::RESTREAM_BOTH,
                                mrl))
                        ).first;
                mountPoint = it->second.get();
            } else {
                JANUS_LOG(LOG_ERR,
                    "Maximum simultaneous streaming sources count (%u) is reached.\n",
                    context.config.maxDynamicMountPoints);
                PushError(
                    context.janus,
                    context.janusPlugin.get(),
                    janusSession,
                    transaction,
                    "maximum simultaneous streaming sources count is reached");

                return;
            }
        } else
            mountPoint = it->second.get();
    }

    if(mountPoint) {
        assert(!session->sdpSessionId);
        if(!session->sdpSessionId)
            session->sdpSessionId.reset(g_strdup_printf("%" PRId64, janus_get_real_time()));

        mountPoint->addWatcher(janusSession, transaction);

        session->watching = mountPoint;

        mountPoint->prepareMedia();
    }
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

    StopWatching(janusSession);
}

static void HandleClientMessage(const ClientMessage& message)
{
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

static void HandleHangupMessage(janus_plugin_session* janusSession)
{
    Session* session = GetSession(janusSession);
    std::unique_ptr<Session> SessionPtr(session);
}

static void HandleDestroyMessage(janus_plugin_session* janusSession)
{
    Session* session = GetSession(janusSession);
    std::unique_ptr<Session> SessionPtr(session);

    StopWatching(janusSession);

    janusSession->plugin_handle = nullptr;
}

static void HandleJanusMessage(const JanusMessage& message)
{
    switch(message.type) {
    case JanusMessage::Type::Hangup:
        HandleHangupMessage(message.janusSessionPtr.get());
        break;
    case JanusMessage::Type::Destroy:
        HandleDestroyMessage(message.janusSessionPtr.get());
        break;
    }
}


static void HandlePluginMessage(const std::unique_ptr<QueueItem>& item, gpointer /*userData*/)
{
    const PluginMessage& message = *static_cast<PluginMessage*>(item.get());
    switch(message.origin) {
    case PluginMessage::Origin::Janus:
        HandleJanusMessage(static_cast<const JanusMessage&>(message));
        break;
    case PluginMessage::Origin::Client:
        HandleClientMessage(static_cast<const ClientMessage&>(message));
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

    context.mountPoints.clear();
    context.dynamicMountPoints.clear();

    context.loopPtr.reset();
    context.mainContextPtr.reset();

    gst_deinit();
}

void StartPluginThread()
{
    PluginContext& context = Context();

    context.mainThread = std::thread(PluginMain);
}

void PostClientMessage(
    janus_plugin_session* janusSession,
    char* transaction,
    json_t* message)
{
    std::unique_ptr<ClientMessage> pluginMessagePtr = std::make_unique<ClientMessage>();
    janus_refcount_increase(&janusSession->ref);
    pluginMessagePtr->janusSessionPtr.reset(janusSession);
    pluginMessagePtr->origin = PluginMessage::Origin::Client;
    if(transaction)
        pluginMessagePtr->transaction = transaction;
    if(message)
        pluginMessagePtr->json.reset(json_incref(message));

    QueueSourcePush(
        Context().queueSourcePtr,
        pluginMessagePtr.release());
}

void PostHangupMessage(
    janus_plugin_session* janusSession)
{
    std::unique_ptr<JanusMessage> janusMessagePtr = std::make_unique<JanusMessage>();
    janus_refcount_increase(&janusSession->ref);
    janusMessagePtr->janusSessionPtr.reset(janusSession);
    janusMessagePtr->origin = PluginMessage::Origin::Janus;
    janusMessagePtr->type = JanusMessage::Type::Hangup;
}

void PostDestroyMessage(
    janus_plugin_session* janusSession)
{
    std::unique_ptr<JanusMessage> janusMessagePtr = std::make_unique<JanusMessage>();
    janus_refcount_increase(&janusSession->ref);
    janusMessagePtr->janusSessionPtr.reset(janusSession);
    janusMessagePtr->origin = PluginMessage::Origin::Janus;
    janusMessagePtr->type = JanusMessage::Type::Destroy;
}
