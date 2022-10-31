#ifndef SRS_APP_HOURGLASS_HPP
#define SRS_APP_HOURGLASS_HPP

#include <srs_core.hpp>
#include <srs_core_time.hpp>

// The handler for fast timer.
class ISrsFastTimer
{
public:
    ISrsFastTimer();
    virtual ~ISrsFastTimer();
public:
    // Tick when timer is active.
    virtual srs_error_t on_timer(srs_utime_t interval) = 0;
};

#endif