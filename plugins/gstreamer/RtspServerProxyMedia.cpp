#include "RtspServerProxyMedia.h"

#include <algorithm>

#include <gst/app/gstappsink.h>

extern "C" {
#include "janus/debug.h"
}

#include "GlibPtr.h"
#include "GstPtr.h"


struct RtspServerProxyMedia::Private
{
    RtspServerProxyMedia *const owner;
};


RtspServerProxyMedia::RtspServerProxyMedia() :
    _p(new Private{.owner = this})
{
}

RtspServerProxyMedia::~RtspServerProxyMedia()
{
    _p.reset();
}

const GstSDPMessage* RtspServerProxyMedia::sdp() const
{
    return nullptr;
}

void RtspServerProxyMedia::doRun()
{
}

void RtspServerProxyMedia::shutdown()
{
}
