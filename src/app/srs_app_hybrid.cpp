#include <srs_app_hybrid.hpp>
#include <srs_app_st.hpp>
#include <srs_kernel_error.hpp>
#include <srs_app_server.hpp>
#include <srs_kernel_log.hpp>

ISrsHybridServer::ISrsHybridServer()
{
}

ISrsHybridServer::~ISrsHybridServer()
{
}

SrsHybridServer::SrsHybridServer()
{
    timer1s_ = new SrsFastTimer("hybrid", 1 * SRS_UTIME_SECONDS);
}

SrsHybridServer::~SrsHybridServer()
{
    srs_freep(timer1s_);
    vector<ISrsHybridServer*>::iterator it;
    for (it = servers.begin(); it != servers.end(); ++it) {
        ISrsHybridServer* server = *it;
        srs_freep(server);
    }
    servers.clear();
}

void SrsHybridServer::register_server(ISrsHybridServer* svr)
{
    servers.push_back(svr);
}

srs_error_t SrsHybridServer::initialize()
{
    srs_error_t err = srs_success;

    // // Start the timer first.
    // if ((err = timer20ms_->start()) != srs_success) {
    //     return srs_error_wrap(err, "start timer");
    // }

    // if ((err = timer100ms_->start()) != srs_success) {
    //     return srs_error_wrap(err, "start timer");
    // }

    if ((err = timer1s_->start()) != srs_success) {
        return srs_error_wrap(err, "start timer");
    }

    // if ((err = timer5s_->start()) != srs_success) {
    //     return srs_error_wrap(err, "start timer");
    // }

    // // Start the DVR async call.
    // if ((err = _srs_dvr_async->start()) != srs_success) {
    //     return srs_error_wrap(err, "dvr async");
    // }

    // // Initialize TencentCloud CLS object.
    // if ((err = _srs_cls->initialize()) != srs_success) {
    //     return srs_error_wrap(err, "cls client");
    // }

    // // Register some timers.
    // timer20ms_->subscribe(clock_monitor_);
    // timer5s_->subscribe(this);

    // Initialize all hybrid servers.
    vector<ISrsHybridServer*>::iterator it;
    for (it = servers.begin(); it != servers.end(); ++it) {
        ISrsHybridServer* server = *it;

        if ((err = server->initialize()) != srs_success) {
            return srs_error_wrap(err, "init server");
        }
    }

    return err;
}

SrsFastTimer* SrsHybridServer::timer1s()
{
    return timer1s_;
}

srs_error_t SrsHybridServer::run()
{
    srs_trace("SrsHybridServer::run()");
    srs_error_t err = srs_success;

    // Wait for all servers which need to do cleanup.
    SrsWaitGroup wg;

    vector<ISrsHybridServer*>::iterator it;
    for (it = servers.begin(); it != servers.end(); ++it) {
        ISrsHybridServer* server = *it;

        if ((err = server->run(&wg)) != srs_success) {
            return srs_error_wrap(err, "run server");
        }
    }

    // Wait for all server to quit.
    wg.wait();

    return err;
}

void SrsHybridServer::stop()
{
    vector<ISrsHybridServer*>::iterator it;
    for (it = servers.begin(); it != servers.end(); ++it) {
        ISrsHybridServer* server = *it;
        server->stop();
    }
}

srs_error_t SrsHybridServer::on_timer(srs_utime_t interval)
{
    srs_error_t err = srs_success;
}

SrsServerAdapter* SrsHybridServer::srs()
{
    for (vector<ISrsHybridServer*>::iterator it = servers.begin(); it != servers.end(); ++it) {
        if (dynamic_cast<SrsServerAdapter*>(*it)) {
            return dynamic_cast<SrsServerAdapter*>(*it);
        }
    }
    return NULL;
}

SrsHybridServer* _srs_hybrid = NULL;