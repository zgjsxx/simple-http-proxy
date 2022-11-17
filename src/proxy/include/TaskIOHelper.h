#pragma once

#include "st.h"

class TaskIOHelper
{
public:
    static bool waitSocketReadable(int sock,st_utime_t timeout);//micro-second
    static bool waitSocketWriteable(int sock,st_utime_t timeout);//micro-second
    static bool waitAnySocket(pollfd* pd, int size, st_utime_t timeout,pollfd &result_pd);
};