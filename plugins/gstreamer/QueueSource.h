#pragma once

#include <memory>

#include <glib.h>


struct QueueItem
{
    virtual ~QueueItem() {}
};

struct QueueSource;

struct QueueSourceUnref
{
    void operator() (QueueSource* queueSource);
};

typedef std::unique_ptr<QueueSource, QueueSourceUnref> QueueSourcePtr;

typedef void (*QueueItemHandleFunc) (const std::unique_ptr<QueueItem>&, gpointer userData);
QueueSourcePtr QueueSourceNew(
    GMainContext* context,
    QueueItemHandleFunc callback,
    gpointer userData);

void QueueSourcePush(QueueSourcePtr&, QueueItem*);
