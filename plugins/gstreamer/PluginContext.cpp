#include "PluginContext.h"


PluginContext& Context()
{
    static PluginContext context;

    return context;
}
