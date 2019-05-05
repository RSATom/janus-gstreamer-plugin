#pragma once

#include <memory>

#include <jansson.h>


struct JanssonUnref
{
    void operator() (json_t* json)
        { json_decref(json); }
};

typedef
    std::unique_ptr<
        json_t,
        JanssonUnref> JsonPtr;
