#include "RtspServer.h"

#include <set>
#include <map>
#include <cassert>

#include "GstRtspServerPtr.h"


struct RtspServer::Private
{
    GstRTSPServerPtr server;
    GstRTSPMountPointsPtr mountPoints;

    void onClientConnected(GstRTSPClient*);

    GstRTSPStatusCode beforePlay(const GstRTSPClient*, const GstRTSPUrl*, const gchar* sessionId);
    void onPlay(const GstRTSPClient*, const GstRTSPUrl*, const gchar* sessionId);

    GstRTSPStatusCode beforeRecord(const GstRTSPClient*, const GstRTSPUrl*, const gchar* sessionId);
    void onRecord(const GstRTSPClient*, const GstRTSPUrl*, const gchar* sessionId);

    void onTeardown(const GstRTSPClient*, const GstRTSPUrl*, const gchar* sessionId);

    void onClientClosed(const GstRTSPClient*);
};


void RtspServer::Private::onClientConnected(GstRTSPClient* client)
{
    GstRTSPConnection* connection =
        gst_rtsp_client_get_connection(client);

    auto prePlayCallback =
        (GstRTSPStatusCode (*)(GstRTSPClient*, GstRTSPContext*, gpointer))
        [] (GstRTSPClient* client, GstRTSPContext* context, gpointer userData) {
            Private* p =
                static_cast<Private*>(userData);
            const gchar* sessionId =
                gst_rtsp_session_get_sessionid(context->session);
            return p->beforePlay(client, context->uri, sessionId);
        };
    g_signal_connect(client, "pre-play-request", GCallback(prePlayCallback), this);

    auto playCallback = (void (*)(GstRTSPClient*, GstRTSPContext*, gpointer))
        [] (GstRTSPClient* client, GstRTSPContext* context, gpointer userData) {
            Private* p =
                static_cast<Private*>(userData);
            const gchar* sessionId =
                gst_rtsp_session_get_sessionid(context->session);
            p->onPlay(client, context->uri, sessionId);
        };
    g_signal_connect(client, "play-request", GCallback(playCallback), this);

    auto preRecordCallback =
        (GstRTSPStatusCode (*)(GstRTSPClient*, GstRTSPContext*, gpointer))
        [] (GstRTSPClient* client, GstRTSPContext* context, gpointer userData) {
            Private* p =
                static_cast<Private*>(userData);
            const gchar* sessionId =
                gst_rtsp_session_get_sessionid(context->session);
            return p->beforeRecord(client, context->uri, sessionId);
        };
    g_signal_connect(client, "pre-record-request", GCallback(preRecordCallback), this);

    auto recordCallback = (void (*)(GstRTSPClient*, GstRTSPContext*, gpointer))
        [] (GstRTSPClient* client, GstRTSPContext* context, gpointer userData) {
            Private* p =
                static_cast<Private*>(userData);
            const gchar* sessionId =
                gst_rtsp_session_get_sessionid(context->session);
            p->onRecord(client, context->uri, sessionId);
        };
    g_signal_connect(client, "record-request", GCallback(recordCallback), this);

    auto teardownCallback = (void (*)(GstRTSPClient*, GstRTSPContext*, gpointer))
        [] (GstRTSPClient* client, GstRTSPContext* context, gpointer userData) {
            Private* p =
                static_cast<Private*>(userData);
            const gchar* sessionId =
                gst_rtsp_session_get_sessionid(context->session);
            p->onTeardown(client, context->uri, sessionId);
        };
    g_signal_connect(client, "teardown-request", GCallback(teardownCallback), this);

    auto closedCallback= (void (*)(GstRTSPClient*, gpointer))
        [] (GstRTSPClient* client, gpointer userData) {
            Private* p =
                static_cast<Private*>(userData);
            p->onClientClosed(client);
        };
    g_signal_connect(client, "closed", GCallback(closedCallback), this);
}

GstRTSPStatusCode RtspServer::Private::beforePlay(
    const GstRTSPClient* client,
    const GstRTSPUrl* url,
    const gchar* sessionId)
{
    return GST_RTSP_STS_FORBIDDEN;
}

void RtspServer::Private::onPlay(
    const GstRTSPClient* client,
    const GstRTSPUrl* url,
    const gchar* sessionId)
{
}

GstRTSPStatusCode RtspServer::Private::beforeRecord(
    const GstRTSPClient* client,
    const GstRTSPUrl* url,
    const gchar* sessionId)
{
    return GST_RTSP_STS_OK;
}

void RtspServer::Private::onRecord(
    const GstRTSPClient* client,
    const GstRTSPUrl* url,
    const gchar* sessionId)
{
}

void RtspServer::Private::onTeardown(
    const GstRTSPClient* client,
    const GstRTSPUrl* url,
    const gchar* sessionId)
{
}

void RtspServer::Private::onClientClosed(const GstRTSPClient* client)
{
}


RtspServer::RtspServer() :
    _p(new Private())
{
    initServer();
}

RtspServer::~RtspServer()
{
    _p.reset();
}

void RtspServer::initServer()
{
    _p->server.reset(gst_rtsp_server_new());
    _p->mountPoints.reset(gst_rtsp_mount_points_new());

    GstRTSPServer* server = _p->server.get();
    GstRTSPMountPoints* mountPoints = _p->mountPoints.get();

    // gst_rtsp_server_set_service(server, );

    gst_rtsp_server_set_mount_points(server, mountPoints);

    auto clientConnectedCallback =
        (void (*)(GstRTSPServer*, GstRTSPClient*, gpointer))
        [] (GstRTSPServer* /*server*/,
            GstRTSPClient* client,
            gpointer userData)
        {
            Private* p =
                static_cast<Private*>(userData);
            p->onClientConnected(client);
        };
    g_signal_connect(
        server, "client-connected",
        (GCallback) clientConnectedCallback, _p.get());
}

void RtspServer::attach()
{
    GstRTSPServer* server = _p->server.get();

    if(!server)
        return;

    gst_rtsp_server_attach(server, g_main_context_get_thread_default());
}

void RtspServer::addMountPoint(const std::string& path, GstRTSPMediaFactory* factory)
{
    gst_rtsp_mount_points_add_factory(_p->mountPoints.get(), path.c_str(), factory);
}
