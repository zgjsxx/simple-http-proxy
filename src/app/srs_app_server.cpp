#include <unistd.h>
#include <fcntl.h>
#include <signal.h> 
#include <srs_app_server.hpp>
#include <srs_core_platform.hpp>
#include <srs_app_config.hpp>
#include <srs_protocol_http_stack.hpp>
#include <srs_app_utility.hpp>
#include <srs_kernel_log.hpp>
#include <srs_app_http_conn.hpp>
#include <srs_kernel_consts.hpp>
#include <srs_app_http_api.hpp>
#include <srs_protocol_log.hpp>

extern SrsConfig* _srs_config;

std::string srs_listener_type2string(SrsListenerType type)
{
    switch (type) {
        case SrsListenerHttpApi:
            return "HTTP-API";
        case SrsListenerHttpsApi:
            return "HTTPS-API";
        case SrsListenerTcp:
            return "TCP";
        case SrsListenerHttpProxy:
            return "HTTP-Proxy";
        default:
            return "UNKONWN";
    }
}

SrsListener::SrsListener(SrsServer* svr, SrsListenerType t)
{
    port = 0;
    server = svr;
    type = t;
}

SrsListener::~SrsListener()
{
}


SrsListenerType SrsListener::listen_type()
{
    return type;
}

SrsBufferListener::SrsBufferListener(SrsServer* svr, SrsListenerType t) : SrsListener(svr, t)
{
    listener = NULL;
}

SrsBufferListener::~SrsBufferListener()
{
    srs_freep(listener);
}

srs_error_t SrsBufferListener::listen(string i, int p)
{
    srs_error_t err = srs_success;
    ip = i;
    port = p;
    
    srs_freep(listener);
    listener = new SrsTcpListener(this, ip, port);

    if ((err = listener->listen()) != srs_success) {
        return srs_error_wrap(err, "buffered tcp listen");
    }
    string v = srs_listener_type2string(type);
    srs_trace("%s listen at tcp://%s:%d, fd=%d", v.c_str(), ip.c_str(), port, listener->fd());
    
    return err;
}

srs_error_t SrsBufferListener::on_tcp_client(srs_netfd_t stfd)
{
    srs_error_t err = server->accept_client(type, stfd);
    if (err != srs_success) {
        srs_warn("accept client failed, err is %s", srs_error_desc(err).c_str());
        srs_freep(err);
    }
    
    return srs_success;
}

SrsSignalManager* SrsSignalManager::instance = NULL;

SrsSignalManager::SrsSignalManager(SrsServer* s)
{
    SrsSignalManager::instance = this;
    
    server = s;
    sig_pipe[0] = sig_pipe[1] = -1;
    trd = new SrsSTCoroutine("signal", this, _srs_context->get_id());
    signal_read_stfd = NULL;
}

SrsSignalManager::~SrsSignalManager()
{
    srs_freep(trd);

    srs_close_stfd(signal_read_stfd);
    
    if (sig_pipe[0] > 0) {
        ::close(sig_pipe[0]);
    }
    if (sig_pipe[1] > 0) {
        ::close(sig_pipe[1]);
    }
}

srs_error_t SrsSignalManager::initialize()
{
    /* Create signal pipe */
    if (pipe(sig_pipe) < 0) {
        return srs_error_new(ERROR_SYSTEM_CREATE_PIPE, "create pipe");
    }
    
    if ((signal_read_stfd = srs_netfd_open(sig_pipe[0])) == NULL) {
        return srs_error_new(ERROR_SYSTEM_CREATE_PIPE, "open pipe");
    }
    
    return srs_success;
}

srs_error_t SrsSignalManager::start()
{
    srs_error_t err = srs_success;
    
    /**
     * Note that if multiple processes are used (see below),
     * the signal pipe should be initialized after the fork(2) call
     * so that each process has its own private pipe.
     */
    struct sigaction sa;
    
    /* Install sig_catcher() as a signal handler */
    sa.sa_handler = SrsSignalManager::sig_catcher;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SRS_SIGNAL_RELOAD, &sa, NULL);
    
    sa.sa_handler = SrsSignalManager::sig_catcher;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SRS_SIGNAL_FAST_QUIT, &sa, NULL);

    sa.sa_handler = SrsSignalManager::sig_catcher;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SRS_SIGNAL_GRACEFULLY_QUIT, &sa, NULL);

    sa.sa_handler = SrsSignalManager::sig_catcher;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SRS_SIGNAL_ASSERT_ABORT, &sa, NULL);
    
    sa.sa_handler = SrsSignalManager::sig_catcher;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    
    sa.sa_handler = SrsSignalManager::sig_catcher;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SRS_SIGNAL_REOPEN_LOG, &sa, NULL);
    
    srs_trace("signal installed, reload=%d, reopen=%d, fast_quit=%d, grace_quit=%d",
              SRS_SIGNAL_RELOAD, SRS_SIGNAL_REOPEN_LOG, SRS_SIGNAL_FAST_QUIT, SRS_SIGNAL_GRACEFULLY_QUIT);
    
    if ((err = trd->start()) != srs_success) {
        return srs_error_wrap(err, "signal manager");
    }
    
    return err;
}

srs_error_t SrsSignalManager::cycle()
{
    srs_error_t err = srs_success;
    
    while (true) {
        if ((err = trd->pull()) != srs_success) {
            return srs_error_wrap(err, "signal manager");
        }
        
        int signo;
        
        /* Read the next signal from the pipe */
        srs_read(signal_read_stfd, &signo, sizeof(int), SRS_UTIME_NO_TIMEOUT);
        
        /* Process signal synchronously */
        // server->on_signal(signo);
    }
    
    return err;
}

void SrsSignalManager::sig_catcher(int signo)
{
    int err;
    
    /* Save errno to restore it after the write() */
    err = errno;
    
    /* write() is reentrant/async-safe */
    int fd = SrsSignalManager::instance->sig_pipe[1];
    write(fd, &signo, sizeof(int));
    
    errno = err;
}

ISrsServerCycle::ISrsServerCycle()
{
}

ISrsServerCycle::~ISrsServerCycle()
{
}

SrsServerAdapter::SrsServerAdapter()
{
    srs = new SrsServer();
}

SrsServerAdapter::~SrsServerAdapter()
{
    srs_freep(srs);
}

srs_error_t SrsServerAdapter::initialize()
{
    srs_error_t err = srs_success;
    return err;
}

srs_error_t SrsServerAdapter::run(SrsWaitGroup* wg)
{
    srs_error_t err = srs_success;

    // Initialize the whole system, set hooks to handle server level events.
    if ((err = srs->initialize(NULL)) != srs_success) {
        return srs_error_wrap(err, "server initialize");
    }

    if ((err = srs->initialize_st()) != srs_success) {
        return srs_error_wrap(err, "initialize st");
    }

    if ((err = srs->initialize_signal()) != srs_success) {
        return srs_error_wrap(err, "initialize signal");
    }

    if ((err = srs->listen()) != srs_success) {
        return srs_error_wrap(err, "listen");
    }

    // if ((err = srs->register_signal()) != srs_success) {
    //     return srs_error_wrap(err, "register signal");
    // }

    if ((err = srs->http_handle()) != srs_success) {
        return srs_error_wrap(err, "http handle");
    }

    if ((err = srs->start(wg)) != srs_success) {
        return srs_error_wrap(err, "start");
    }

    return err;
}

void SrsServerAdapter::stop()
{
}

SrsServer* SrsServerAdapter::instance()
{
    return srs;
}

SrsServer::SrsServer()
{
    signal_reload = false;
    signal_persistence_config = false;
    signal_gmc_stop = false;
    signal_fast_quit = false;
    signal_gracefully_quit = false;
    pid_fd = -1;

    signal_manager = new SrsSignalManager(this);
    conn_manager = new SrsResourceManager("TCP", true);
    // donot new object in constructor,
    // for some global instance is not ready now,
    // new these objects in initialize instead.
    http_api_mux = new SrsHttpServeMux();
    http_server = new SrsHttpServer(this);
    trd_ = new SrsSTCoroutine("srs", this, _srs_context->get_id());
    reuse_api_over_server_ = false;
}

SrsServer::~SrsServer()
{
    destroy();
}

void SrsServer::destroy()
{
    srs_warn("start destroy server");
    
    dispose();

    srs_freep(trd_);
    srs_freep(signal_manager);
    srs_freep(conn_manager);
    srs_freep(http_server);
    srs_freep(http_api_mux);
}

void SrsServer::dispose()
{
    // _srs_config->unsubscribe(this);
    
    // prevent fresh clients.
    close_listeners(SrsListenerRtmpStream);
    close_listeners(SrsListenerHttpApi);
    close_listeners(SrsListenerHttpsApi);
    close_listeners(SrsListenerHttpStream);
    close_listeners(SrsListenerHttpsStream);
    close_listeners(SrsListenerMpegTsOverUdp);
    close_listeners(SrsListenerFlv);
    close_listeners(SrsListenerTcp);

}

srs_error_t SrsServer::initialize(ISrsServerCycle* ch)
{
    srs_error_t err = srs_success;

    // for the main objects(server, config, log, context),
    // never subscribe handler in constructor,
    // instead, subscribe handler in initialize method.
    srs_assert(_srs_config);
    handler = ch;
    if(handler && (err = handler->initialize()) != srs_success){
        return srs_error_wrap(err, "handler initialize");
    }
    bool stream = _srs_config->get_http_stream_enabled();
    string http_listen = _srs_config->get_http_stream_listen();
    string https_listen = _srs_config->get_https_stream_listen();

// #ifdef SRS_RTC
//     bool rtc = _srs_config->get_rtc_server_enabled();
//     bool rtc_tcp = _srs_config->get_rtc_server_tcp_enabled();
//     string rtc_listen = srs_int2str(_srs_config->get_rtc_server_tcp_listen());
//     // If enabled and listen is the same value, resue port for WebRTC over TCP.
//     if (stream && rtc && rtc_tcp && http_listen == rtc_listen) {
//         srs_trace("WebRTC tcp=%s reuses http=%s server", rtc_listen.c_str(), http_listen.c_str());
//         reuse_rtc_over_server_ = true;
//     }
//     if (stream && rtc && rtc_tcp && https_listen == rtc_listen) {
//         srs_trace("WebRTC tcp=%s reuses https=%s server", rtc_listen.c_str(), https_listen.c_str());
//         reuse_rtc_over_server_ = true;
//     }
// #endif


    // If enabled and the listen is the same value, reuse port.
    bool api = _srs_config->get_http_api_enabled();
    string api_listen = _srs_config->get_http_api_listen();
    string apis_listen = _srs_config->get_https_api_listen();
    if (stream && api && api_listen == http_listen && apis_listen == https_listen) {
        srs_trace("API reuses http=%s and https=%s server", http_listen.c_str(), https_listen.c_str());
        reuse_api_over_server_ = true;
    }

    // Only init HTTP API when not reusing HTTP server.
    if (!reuse_api_over_server_) {
        SrsHttpServeMux *api = dynamic_cast<SrsHttpServeMux*>(http_api_mux);
        srs_assert(api);
        srs_trace("srs_assert(api)");
        if ((err = api->initialize()) != srs_success) {
            srs_trace("err = api->initialize()) != srs_success");
            return srs_error_wrap(err, "http api initialize");
        }
    } else {
        srs_freep(http_api_mux);
        http_api_mux = http_server;
    }

    srs_trace("http_server->initialize()");
    if ((err = http_server->initialize()) != srs_success) {
        srs_trace("err = http_server->initialize()) != srs_success");
        return srs_error_wrap(err, "http server initialize");
    }

    return err;
}

srs_error_t SrsServer::cycle()
{
    srs_error_t err = srs_success;

    // Start the inotify auto reload by watching config file.
    // SrsInotifyWorker inotify(this);
    // if ((err = inotify.start()) != srs_success) {
        // return srs_error_wrap(err, "start inotify");
    // }

    // Do server main cycle.
     err = do_cycle();

    // OK, SRS server is done.
    wg_->done();

    return err;
}

srs_error_t SrsServer::do_cycle()
{
    srs_error_t err = srs_success;
    
    // for asprocess.
    bool asprocess = _srs_config->get_asprocess();

    while (true) {
        if ((err = trd_->pull()) != srs_success) {
            return srs_error_wrap(err, "pull");
        }

        // gracefully quit for SIGINT or SIGTERM or SIGQUIT.
        if (signal_fast_quit || signal_gracefully_quit) {
            srs_trace("cleanup for quit signal fast=%d, grace=%d", signal_fast_quit, signal_gracefully_quit);
            return err;
        }
        srs_trace("SrsServer::do_cycle");
        srs_trace("current free stack number= %d", srs_get_current_free_stack());

        // do reload the config.
        if (signal_reload) {
            signal_reload = false;
            srs_info("get signal to reload the config.");

            if ((err = _srs_config->reload()) != srs_success) {
                return srs_error_wrap(err, "config reload");
            }
            srs_trace("reload config success.");
        }
        
        srs_usleep(1 * SRS_UTIME_SECONDS);
    }
    return err;
}

srs_error_t SrsServer::initialize_st()
{
    srs_error_t err = srs_success;

    // check asprocess.
    bool asprocess = _srs_config->get_asprocess();
    if (asprocess && ppid == 1) {
        return srs_error_new(ERROR_SYSTEM_ASSERT_FAILED, "ppid=%d illegal for asprocess", ppid);
    }
    
    srs_trace("server main cid=%s, pid=%d, ppid=%d, asprocess=%d",
        _srs_context->get_id().c_str(), ::getpid(), ppid, asprocess);
    
    return err;
}

srs_error_t SrsServer::initialize_signal()
{
    srs_error_t err = srs_success;

    if ((err = signal_manager->initialize()) != srs_success) {
        return srs_error_wrap(err, "init signal manager");
    }

    // Start the version query coroutine.
    // if ((err = latest_version_->start()) != srs_success) {
    //     return srs_error_wrap(err, "start version query");
    // }

    return err;
}

srs_error_t SrsServer::register_signal()
{
    srs_error_t err = srs_success;
    
    if ((err = signal_manager->start()) != srs_success) {
        return srs_error_wrap(err, "signal manager start");
    }
    
    return err;
}


srs_error_t SrsServer::listen()
{
    srs_error_t err = srs_success;
    if ((err = listen_http_api()) != srs_success) {
        return srs_error_wrap(err, "http api listen");
    }
    
    if ((err = listen_https_api()) != srs_success) {
        return srs_error_wrap(err, "https api listen");
    }

    if((err = listen_http_proxy()) != srs_success) {
        return srs_error_wrap(err, "http proxy listen");
    }

    if ((err = conn_manager->start()) != srs_success) {
        return srs_error_wrap(err, "connection manager");
    }

    return err;
}

srs_error_t SrsServer::http_handle()
{
    srs_error_t err = srs_success;

    // Ignore / and /api/v1/versions for already handled by HTTP server.
    // if (!reuse_api_over_server_) {
    //     if ((err = http_api_mux->handle("/", new SrsGoApiRoot())) != srs_success) {
    //         return srs_error_wrap(err, "handle /");
    //     }
    //     if ((err = http_api_mux->handle("/api/v1/versions", new SrsGoApiVersion())) != srs_success) {
    //         return srs_error_wrap(err, "handle versions");
    //     }
    // }


    if ((err = http_api_mux->handle("/api/", new SrsGoApiApi())) != srs_success) {
        return srs_error_wrap(err, "handle api");
    }
    if ((err = http_api_mux->handle("/api/mytest", new SrsMytest())) != srs_success) {
        return srs_error_wrap(err, "handle mytest");
    }
    if ((err = http_api_mux->handle("/api/v1/self_proc_stats", new SrsGoApiSelfProcStats())) != srs_success) {
        return srs_error_wrap(err, "handle self proc stats");
    }

    return err;
}

srs_error_t SrsServer::start(SrsWaitGroup* wg)
{
    srs_error_t err = srs_success;

    // if ((err = _srs_sources->initialize()) != srs_success) {
    //     return srs_error_wrap(err, "sources");
    // }

    if ((err = trd_->start()) != srs_success) {
        return srs_error_wrap(err, "start");
    }

    // if ((err = setup_ticks()) != srs_success) {
    //     return srs_error_wrap(err, "tick");
    // }

    // OK, we start SRS server.
    wg_ = wg;
    wg->add(1);

    return err;
}

void SrsServer::close_listeners(SrsListenerType type)
{
    std::vector<SrsListener*>::iterator it;
    for (it = listeners.begin(); it != listeners.end();) {
        SrsListener* listener = *it;
        
        if (listener->listen_type() != type) {
            ++it;
            continue;
        }
        
        srs_freep(listener);
        it = listeners.erase(it);
    }
}


void SrsServer::on_signal(int signo)
{
    // For signal to quit with coredump.
    if (signo == SRS_SIGNAL_ASSERT_ABORT) {
        srs_trace("abort with coredump, signo=%d", signo);
        srs_assert(false);
        return;
    }

    if (signo == SRS_SIGNAL_RELOAD) {
        srs_trace("reload config, signo=%d", signo);
        signal_reload = true;
        return;
    }
// #ifndef SRS_GPERF_MC
//     if (signo == SRS_SIGNAL_REOPEN_LOG) {
//         _srs_log->reopen();

//         if (handler) {
//             handler->on_logrotate();
//         }

//         srs_warn("reopen log file, signo=%d", signo);
//         return;
//     }
// #endif
    
// #ifdef SRS_GPERF_MC
//     if (signo == SRS_SIGNAL_REOPEN_LOG) {
//         signal_gmc_stop = true;
//         srs_warn("for gmc, the SIGUSR1 used as SIGINT, signo=%d", signo);
//         return;
//     }
// #endif    
    if (signo == SRS_SIGNAL_PERSISTENCE_CONFIG) {
        signal_persistence_config = true;
        return;
    }

    if (signo == SIGINT) {
#ifdef SRS_GPERF_MC
        srs_trace("gmc is on, main cycle will terminate normally, signo=%d", signo);
        signal_gmc_stop = true;
#endif
    }  

    // For K8S, force to gracefully quit for gray release or canary.
    // @see https://github.com/ossrs/srs/issues/1595#issuecomment-587473037
    // if (signo == SRS_SIGNAL_FAST_QUIT && _srs_config->is_force_grace_quit()) {
    //     srs_trace("force gracefully quit, signo=%d", signo);
    //     signo = SRS_SIGNAL_GRACEFULLY_QUIT;
    // }

    // if ((signo == SIGINT || signo == SRS_SIGNAL_FAST_QUIT) && !signal_fast_quit) {
    //     srs_trace("sig=%d, user terminate program, fast quit", signo);
    //     signal_fast_quit = true;
    //     return;
    // }

    if (signo == SRS_SIGNAL_GRACEFULLY_QUIT && !signal_gracefully_quit) {
        srs_trace("sig=%d, user start gracefully quit", signo);
        signal_gracefully_quit = true;
        return;
    }

}

srs_error_t SrsServer::listen_http_api()
{
    srs_error_t err = srs_success;

    close_listeners(SrsListenerHttpApi);
    
    // Ignore if not enabled.
    if (!_srs_config->get_http_api_enabled()) {
        return err;
    }
    // Ignore if reuse same port to http server.
    if (reuse_api_over_server_) {
        srs_trace("HTTP-API: Reuse listen to http server %s", _srs_config->get_http_stream_listen().c_str());
        return err;
    }
    // Listen at a dedicated HTTP API endpoint.
    SrsListener* listener = new SrsBufferListener(this, SrsListenerHttpApi);
    listeners.push_back(listener);

    std::string ep = _srs_config->get_http_api_listen();

    std::string ip;
    int port;
    srs_parse_endpoint(ep, ip, port);

    if ((err = listener->listen(ip, port)) != srs_success) {
        return srs_error_wrap(err, "http api listen %s:%d", ip.c_str(), port);
    }

    return err;
}

srs_error_t SrsServer::listen_https_api()
{
    srs_error_t err = srs_success;

    close_listeners(SrsListenerHttpsApi);

    // Ignore if not enabled.
    if (!_srs_config->get_https_api_enabled()) {
        return err;
    }

    // Ignore if reuse same port to https server.
    if (reuse_api_over_server_) {
        srs_trace("HTTPS-API: Reuse listen to https server %s", _srs_config->get_https_stream_listen().c_str());
        return err;
    }

    // Listen at a dedicated HTTPS API endpoint.
    SrsListener* listener = new SrsBufferListener(this, SrsListenerHttpsApi);
    listeners.push_back(listener);

    std::string ep = _srs_config->get_https_api_listen();

    std::string ip;
    int port;
    srs_parse_endpoint(ep, ip, port);

    if ((err = listener->listen(ip, port)) != srs_success) {
        return srs_error_wrap(err, "https api listen %s:%d", ip.c_str(), port);
    }

    return err;
}
srs_error_t SrsServer::listen_http_proxy()
{
    srs_error_t err = srs_success;

    close_listeners(SrsListenerHttpProxy);

    // Ignore if not enabled.
    if (!_srs_config->get_http_proxy_enabled()) {
        return err;
    }

    // Listen at a dedicated HTTPS API endpoint.
    SrsListener* listener = new SrsBufferListener(this, SrsListenerHttpProxy);
    listeners.push_back(listener);

    std::string ep = _srs_config->get_http_proxy_listen();

    std::string ip;
    int port;
    srs_parse_endpoint(ep, ip, port);

    if ((err = listener->listen(ip, port)) != srs_success) {
        return srs_error_wrap(err, "http proxy listen %s:%d", ip.c_str(), port);
    }

    return err; 
}

srs_error_t SrsServer::accept_client(SrsListenerType type, srs_netfd_t stfd)
{
    srs_error_t err = srs_success;
    
    ISrsResource* resource = NULL;
    if ((err = fd_to_resource(type, stfd, &resource)) != srs_success) {
        srs_close_stfd(stfd);
        if (srs_error_code(err) == ERROR_SOCKET_GET_PEER_IP && _srs_config->empty_ip_ok()) {
            srs_error_reset(err);
            return srs_success;
        }
        return srs_error_wrap(err, "fd to resource");
    }
    // Ignore if no resource found.
    if (!resource) {
        return err;
    }
    
    // directly enqueue, the cycle thread will remove the client.
    conn_manager->add(resource);
    srs_trace("conn start");
    ISrsStartable* conn = dynamic_cast<ISrsStartable*>(resource);
    if ((err = conn->start()) != srs_success) {
        return srs_error_wrap(err, "start conn coroutine");
    }
    
    return err;    
}

srs_error_t SrsServer::fd_to_resource(SrsListenerType type, srs_netfd_t& stfd, ISrsResource** pr)
{
    srs_error_t err = srs_success;
    int fd = srs_netfd_fileno(stfd);
    string ip = srs_get_peer_ip(fd);
    int port = srs_get_peer_port(fd);  
    // for some keep alive application, for example, the keepalived,
    // will send some tcp packet which we cann't got the ip,
    // we just ignore it.
    if (ip.empty()) {
        return srs_error_new(ERROR_SOCKET_GET_PEER_IP, "ignore empty ip, fd=%d", fd);
    }

    // check connection limitation.
    // int max_connections = _srs_config->get_max_connections();
    // if (handler && (err = handler->on_accept_client(max_connections, (int)conn_manager->size())) != srs_success) {
    //     return srs_error_wrap(err, "drop client fd=%d, ip=%s:%d, max=%d, cur=%d for err: %s",
    //         fd, ip.c_str(), port, max_connections, (int)conn_manager->size(), srs_error_desc(err).c_str());
    // }
    // if ((int)conn_manager->size() >= max_connections) {
    //     return srs_error_new(ERROR_EXCEED_CONNECTIONS, "drop fd=%d, ip=%s:%d, max=%d, cur=%d for exceed connection limits",
    //         fd, ip.c_str(), port, max_connections, (int)conn_manager->size());
    // }  
    // avoid fd leak when fork.
    // @see https://github.com/ossrs/srs/issues/518
    if (true) {
        int val;
        if ((val = fcntl(fd, F_GETFD, 0)) < 0) {
            return srs_error_new(ERROR_SYSTEM_PID_GET_FILE_INFO, "fnctl F_GETFD error! fd=%d", fd);
        }
        val |= FD_CLOEXEC;
        if (fcntl(fd, F_SETFD, val) < 0) {
            return srs_error_new(ERROR_SYSTEM_PID_SET_FILE_INFO, "fcntl F_SETFD error! fd=%d", fd);
        }
    }
    // We will free the stfd from now on.
    srs_netfd_t fd2 = stfd;
    stfd = NULL;

    // The context id may change during creating the bellow objects.
    SrsContextRestore(_srs_context->get_id());    
    if (type == SrsListenerHttpApi || type == SrsListenerHttpsApi) {
        *pr = new SrsHttpxConn(type == SrsListenerHttpsApi, this, new SrsTcpConnection(fd2), http_api_mux, ip, port);
    } 
    else if(type == SrsListenerHttpProxy){
        *pr = new SrsHttpxProxyConn(new SrsTcpConnection(fd2), this, http_api_mux, ip, port);
    } 
    else {
        srs_warn("close for no service handler. fd=%d, ip=%s:%d", fd, ip.c_str(), port);
        srs_close_stfd(fd2);
        return err;
    }
    
    return err;
}

void SrsServer::remove(ISrsResource* c)
{
    // use manager to free it async.
    conn_manager->remove(c);
}