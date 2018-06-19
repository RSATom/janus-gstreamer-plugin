#include "Request.h"

extern "C" {
#include "janus/debug.h"
}

#include "PluginContext.h"


Request ParseRequest(const json_t* message)
{
    const char* strRequest = nullptr;
    if(json_t* jsonRequest = json_object_get(message, "request"))
        strRequest = json_string_value(jsonRequest);

    if(!strRequest) {
        JANUS_LOG(LOG_ERR, "%s: no request\n", GetPluginName());
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
        JANUS_LOG(LOG_ERR, "%s: unsupported request \"%s\"\n", GetPluginName(), strRequest);
        return Request::Invalid;
    }
}

Request ParseRequest(JsonPtr& message)
{
    return ParseRequest(message.get());
}
