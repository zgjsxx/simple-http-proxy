#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <srs_core.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_log.hpp>
#include <srs_app_threads.hpp>
#include <srs_app_config.hpp>
#include <srs_app_hybrid.hpp>
#include <srs_app_server.hpp>
#include <srs_app_policy.hpp>
#include <srs_app_access_log.hpp>
#ifdef SRS_GPERF_CP
#include <gperftools/profiler.h>
#endif

// @global log and context.
ISrsLog* _srs_log = NULL;
// It SHOULD be thread-safe, because it use thread-local thread private data.
ISrsContext* _srs_context = NULL;
// @global config object for app module.
SrsConfig* _srs_config = NULL;
// @global policy object for app module
SrsPolicy* _srs_policy = NULL;
// @global notification object for app module
SrsNotification* _srs_notification = NULL;

SrsAccessLog* _srs_access_log = NULL;
#include <iostream>
using namespace std;

srs_error_t run_hybrid_server(void* arg);
srs_error_t run_in_thread_pool()
{
    srs_error_t err = srs_success;

    // Initialize the thread pool.
    if ((err = _srs_thread_pool->initialize()) != srs_success) {
        return srs_error_wrap(err, "init thread pool");
    }

    srs_trace("run_in_thread_pool");

    // Start the hybrid service worker thread, for RTMP and RTC server, etc.
    if ((err = _srs_thread_pool->execute("hybrid", run_hybrid_server, (void*)NULL)) != srs_success) {
        return srs_error_wrap(err, "start hybrid server thread");
    }

    srs_trace("Pool: Start threads primordial=1, hybrids=1 ok");

    return _srs_thread_pool->run();
}

srs_error_t run_hybrid_server(void* /*arg*/)
{
    srs_error_t err = srs_success;
    srs_trace("run_hybrid_server");

    // Create servers and register them.
    _srs_hybrid->register_server(new SrsServerAdapter());
   
    // Do some system initialize.
    if ((err = _srs_hybrid->initialize()) != srs_success) {
        return srs_error_wrap(err, "hybrid initialize");
    }
    // Should run util hybrid servers all done.
    if ((err = _srs_hybrid->run()) != srs_success) {
        return srs_error_wrap(err, "hybrid run");
    }

    // After all done, stop and cleanup.
    _srs_hybrid->stop();

    return err;
}

srs_error_t run_directly_or_daemon()
{
    srs_error_t err = srs_success;
    srs_trace("start daemon mode...");
    int pid = fork();
    if(pid < 0) {
        return srs_error_new(-1, "fork father process");
    }   

    // run directly
    // if ((err = run_in_thread_pool()) != srs_success) {
    //     return srs_error_wrap(err, "daemon run thread pool");
    // }

    // run with daemon

    // grandpa
    if(pid > 0) {
        int status = 0;
        waitpid(pid, &status, 0);
        // srs_trace("grandpa process exit.");
        exit(0);
    }
    // father
    pid = fork();
    if(pid < 0) {
        return srs_error_new(-1, "fork child process");
    }
    
    if(pid > 0) {
        srs_trace("father process exit");
        exit(0);
    }
    srs_trace("son(daemon) process running.");
    
    if ((err = run_in_thread_pool()) != srs_success) {
        return srs_error_wrap(err, "daemon run thread pool");
    }
    return err;
}

srs_error_t do_main(int argc, char** argv)
{
    srs_error_t err = srs_success;
    // Initialize global and thread-local variables.
    if ((err = srs_global_initialize()) != srs_success) {
        return srs_error_wrap(err, "global init");
    }

    if ((err = SrsThreadPool::setup_thread_locals()) != srs_success) {
        return srs_error_wrap(err, "thread init");
    }

#ifdef SRS_GPERF_CP
    ProfilerStart("gperf.srs.gcp");
#endif

    // For background context id.
    // never use srs log(srs_trace, srs_error, etc) before config parse the option,
    // which will load the log config and apply it.
    if ((err = _srs_config->parse_options(argc, argv)) != srs_success) {

        return srs_error_wrap(err, "config parse options");
    }
   
    _srs_context->set_id(_srs_context->generate_id());
    // config parsed, initialize log.
    if ((err = _srs_log->initialize()) != srs_success) {
        return srs_error_wrap(err, "log initialize");
    }

    srs_trace("configure detail: ");

    if ((err = run_directly_or_daemon()) != srs_success) {
        return srs_error_wrap(err, "run");
    }

    return err;
}

int main(int argc, char** argv)
{
    srs_error_t err = do_main(argc, argv);
    if(err != srs_success) {
        srs_freep(err);
    }

    return 0;
}