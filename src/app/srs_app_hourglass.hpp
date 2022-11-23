#ifndef SRS_APP_HOURGLASS_HPP
#define SRS_APP_HOURGLASS_HPP

#include <vector>

#include <srs_core.hpp>
#include <srs_core_time.hpp>
#include <srs_app_st.hpp>

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

// The fast timer, shared by objects, for high performance.
// For example, we should never start a timer for each connection or publisher or player,
// instead, we should start only one fast timer in server.
class SrsFastTimer : public ISrsCoroutineHandler
{
private:
    SrsCoroutine* trd_;
    srs_utime_t interval_;
    std::vector<ISrsFastTimer*> handlers_;
public:
    SrsFastTimer(std::string label, srs_utime_t interval);
    virtual ~SrsFastTimer();
public:
    srs_error_t start();
public:
    void subscribe(ISrsFastTimer* timer);
    void unsubscribe(ISrsFastTimer* timer);
// Interface ISrsCoroutineHandler
private:
    // Cycle the hourglass, which will sleep resolution every time.
    // and call handler when ticked.
    virtual srs_error_t cycle();
};

#endif