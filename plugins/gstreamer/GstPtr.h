#pragma once

#include <memory>

#include <gst/gst.h>


struct GstUnref
{
    void operator() (GObject* object)
        { gst_object_unref(object); }

    void operator() (GstBus* bus)
        { (*this)(G_OBJECT(bus)); }

    void operator() (GstCaps* caps)
        { gst_caps_unref(caps); }

    void operator() (GstElement* element)
        { (*this)(G_OBJECT(element)); }

    void operator() (GstPad* pad)
        { (*this)(G_OBJECT(pad)); }

    void operator() (GstSample* sample)
        { gst_sample_unref(sample); }

    void operator() (GstBuffer* buffer)
        { gst_buffer_unref(buffer); }

    void operator() (GstSDPMessage* sdp)
        { gst_sdp_message_free(sdp); }

};

typedef
    std::unique_ptr<
        GstBus,
        GstUnref> GstBusPtr;
typedef
    std::unique_ptr<
        GstCaps,
        GstUnref> GstCapsPtr;
typedef
    std::unique_ptr<
        GstElement,
        GstUnref> GstElementPtr;
typedef
    std::unique_ptr<
        GstPad,
        GstUnref> GstPadPtr;
typedef
    std::unique_ptr<
        GstSample,
        GstUnref> GstSamplePtr;
typedef
    std::unique_ptr<
        GstBuffer,
        GstUnref> GstBufferPtr;
typedef
    std::unique_ptr<
        GstSDPMessage,
        GstUnref> GstSDPMessagePtr;
