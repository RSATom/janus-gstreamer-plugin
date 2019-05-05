#include "Session.h"

#include "JanssonPtr.h"


void PushError(
    janus_callbacks* janus,
    janus_plugin* plugin,
    janus_plugin_session* janusSession,
    const std::string& transaction,
    const char* errorText)
{
    JANUS_LOG(LOG_ERR, "%s\n", errorText);

    JsonPtr eventPtr(json_object());
    json_t* event = eventPtr.get();

    json_object_set_new(event, "error", json_string(errorText));

    janus->push_event(
        janusSession, plugin,
        transaction.c_str(), event, nullptr);
}
