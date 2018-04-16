#include "QueueSource.h"

#include <sys/eventfd.h>
#include <unistd.h>


struct QueueSource
{
    GSource base;

    int notify_fd;
    gpointer notify_fd_tag;

    GAsyncQueue* queue;
};

void QueueSourceUnref::operator() (QueueSource* queueSource)
{
    g_source_unref(reinterpret_cast<GSource*>(queueSource));
}

static gboolean prepare(GSource* source, gint* timeout)
{
    QueueSource* queueSource = reinterpret_cast<QueueSource*>(source);

    *timeout = -1;

    return (g_async_queue_length(queueSource->queue) > 0);
}

static gboolean check(GSource* source)
{
    QueueSource* queueSource = reinterpret_cast<QueueSource*>(source);

    eventfd_t value;
    eventfd_read(queueSource->notify_fd, &value);

    return (g_async_queue_length(queueSource->queue) > 0);
}

static gboolean dispatch(GSource* source,
                         GSourceFunc sourceCallback,
                         gpointer userData)
{
    QueueSource* queueSource = reinterpret_cast<QueueSource*>(source);

    QueueItemHandleFunc callback = reinterpret_cast<QueueItemHandleFunc>(sourceCallback);

    if(gpointer item = g_async_queue_try_pop(queueSource->queue))
        callback(std::unique_ptr<QueueItem>(static_cast<QueueItem*>(item)), userData);

    return G_SOURCE_CONTINUE;
}

static void finalize(GSource* source)
{
    QueueSource* queueSource = reinterpret_cast<QueueSource*>(source);

    g_source_remove_unix_fd(source, queueSource->notify_fd_tag);
    queueSource->notify_fd_tag = nullptr;

    close(queueSource->notify_fd);
    queueSource->notify_fd = -1;

    g_async_queue_unref(queueSource->queue);
    queueSource->queue = nullptr;
}

QueueSourcePtr QueueSourceNew(
    GMainContext* context,
    QueueItemHandleFunc callback,
    gpointer userData)
{
    g_return_val_if_fail(context != nullptr, nullptr);
    g_return_val_if_fail(callback != nullptr, nullptr);

    static GSourceFuncs funcs = {
        .prepare = prepare,
        .check = check,
        .dispatch = dispatch,
        .finalize = finalize,
    };

    GSource* source = g_source_new(&funcs, sizeof(QueueSource));
    QueueSource* queueSource = reinterpret_cast<QueueSource*>(source);
    g_source_attach(source, context);

    queueSource->notify_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    queueSource->queue = g_async_queue_new();
    queueSource->notify_fd_tag = g_source_add_unix_fd(source, queueSource->notify_fd, G_IO_IN);

    g_source_set_callback(source, reinterpret_cast<GSourceFunc>(callback), userData, nullptr);

    return QueueSourcePtr(queueSource);
}

void QueueSourcePush(QueueSourcePtr& queueSourcePtr, QueueItem* item)
{
    g_return_if_fail(item != nullptr);

    g_async_queue_push(queueSourcePtr->queue, item);

    eventfd_write(queueSourcePtr->notify_fd, 1);
}
