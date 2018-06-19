#include <memory>
#include <cassert>

#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>
#include <gst/app/gstappsink.h>

extern "C" {
#include "janus/plugins/plugin.h"
#include "janus/debug.h"
}

#include "GstPtr.h"
#include "JsonPtr.h"
#include "Session.h"
#include "ConfigLoader.h"
#include "PluginContext.h"
#include "Request.h"
#include "PluginMain.h"


namespace
{

enum {
    PLUGIN_VERSION = 1,
};

enum {
    INVALID_JSON_ERROR = 451,
};

}


static const char* PluginStringVersion = "0.0.1";
static const char* PluginDescription   = "Gstreamer plugin";
static const char* PluginName          = "Gstreamer plugin";
static const char* PluginAuthor        = "Sergey Radionov";
static const char* PluginPackage       = "janus.plugin.gstreamer";

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

    PluginContext& context = Context();

    context.janusPlugin =
        std::make_unique<janus_plugin>(janus_plugin {
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
        });

    return context.janusPlugin.get();
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

static int Init(janus_callbacks* callback, const char* configPath)
{
    JANUS_LOG(LOG_DBG, ">>>> %s: init\n", PluginName);

    PluginContext& context = Context();

    context.janus = callback;

    LoadConfig(
        context.janus, context.janusPlugin.get(),
        std::string(configPath) + "/" + PluginPackage + ".jcfg",
        &context.mountPoints);

    StartPluginThread();

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
        json_object_set_new(listItem, "description", json_string(pair.second->description().c_str()));
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
