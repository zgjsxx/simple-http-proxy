#ifndef SRS_APP_HYBRID_HPP
#define SRS_APP_HYBRID_HPP

#include <vector>

#include <srs_core.hpp>
#include <srs_app_hourglass.hpp>

using std::vector;

class SrsServerAdapter;
class SrsWaitGroup;

class ISrsHybridServer
{
public:
    ISrsHybridServer();
    virtual ~ISrsHybridServer();
public:
    virtual srs_error_t initialize() = 0;
    // Run each server, should never block except the SRS master server.
    virtual srs_error_t run(SrsWaitGroup* wg) = 0;
    // Stop each server, should do cleanup, for example, kill processes forked by server.
    virtual void stop() = 0;    
};

// The hybrid server manager.
class SrsHybridServer : public ISrsFastTimer
{
private:
    std::vector<ISrsHybridServer*> servers;
    SrsFastTimer* timer1s_;
public:
    SrsHybridServer();
    virtual ~SrsHybridServer();
public:
    virtual void register_server(ISrsHybridServer* svr);
public:
    virtual srs_error_t initialize();
    virtual srs_error_t run();
    virtual void stop();
public:
    virtual SrsServerAdapter* srs();
    SrsFastTimer* timer1s();
// interface ISrsFastTimer
private:
    srs_error_t on_timer(srs_utime_t interval);
};

extern SrsHybridServer* _srs_hybrid;

#endif