#pragma once

#include "CxxPtr/JanssonPtr.h"


enum class Request
{
    Invalid,
    List,
    Watch,
    Start,
    Stop,
};

Request ParseRequest(const json_t* message);
Request ParseRequest(const JsonPtr& message);
