#ifndef SRS_APP_SERVER_HPP
#define SRS_APP_SERVER_HPP

#include <string>
#include <srs_core.hpp>
#include <srs_app_st.hpp>
#include <srs_app_listener.hpp>
#include <srs_app_hybrid.hpp>
#include <srs_protocol_conn.hpp>
#include <srs_app_conn.hpp>

class SrsServer;
class ISrsHttpServeMux;
class SrsHttpServer;

// The listener type for server to identify the connection,
// that is, use different type to process the connection.
enum SrsListenerType
{
    // RTMP client,
    SrsListenerRtmpStream = 0,
    // HTTP api,
    SrsListenerHttpApi = 1,
    // HTTP stream, HDS/HLS/DASH
    SrsListenerHttpStream = 2,
    // UDP stream, MPEG-TS over udp.
    SrsListenerMpegTsOverUdp = 3,
    // TCP stream, FLV stream over HTTP.
    SrsListenerFlv = 5,
    // HTTPS api,
    SrsListenerHttpsApi = 8,
    // HTTPS stream,
    SrsListenerHttpsStream = 9,
    // WebRTC over TCP,
    SrsListenerTcp = 10,
    //srs proxy connection
    SrsListenerHttpProxy = 11
};

class SrsListener
{
protected:
    SrsListenerType type;
protected:
    std::string ip;
    int port;
    SrsServer* server;
public:
    SrsListener(SrsServer* svr, SrsListenerType t);
    virtual ~SrsListener();
public:
    virtual SrsListenerType listen_type();
    virtual srs_error_t listen(std::string i, int p) = 0;
};

// A buffered TCP listener.
class SrsBufferListener : public SrsListener, public ISrsTcpHandler
{
private:
    SrsTcpListener* listener;
public:
    SrsBufferListener(SrsServer* server, SrsListenerType type);
    virtual ~SrsBufferListener();
public:
    virtual srs_error_t listen(std::string ip, int port);
// Interface ISrsTcpHandler
public:
    virtual srs_error_t on_tcp_client(srs_netfd_t stfd);
};

// Convert signal to io,
// @see: st-1.9/docs/notes.html
class SrsSignalManager : public ISrsCoroutineHandler
{
private:
    // Per-process pipe which is used as a signal queue.
    // Up to PIPE_BUF/sizeof(int) signals can be queued up.
    int sig_pipe[2];
    srs_netfd_t signal_read_stfd;
private:
    SrsServer* server;
    SrsCoroutine* trd;
public:
    SrsSignalManager(SrsServer* s);
    virtual ~SrsSignalManager();
public:
    virtual srs_error_t initialize();
    virtual srs_error_t start();
// Interface ISrsEndlessThreadHandler.
public:
    virtual srs_error_t cycle();
private:
    // Global singleton instance
    static SrsSignalManager* instance;
    // Signal catching function.
    // Converts signal event to I/O event.
    static void sig_catcher(int signo);    
};


// A handler to the handle cycle in SRS RTMP server.
class ISrsServerCycle
{
public:
    ISrsServerCycle();
    virtual ~ISrsServerCycle();
public:
    // Initialize the cycle handler.
    virtual srs_error_t initialize() = 0;
    // Do on_cycle while server doing cycle.
    virtual srs_error_t on_cycle() = 0;
    // Callback the handler when got client.
    virtual srs_error_t on_accept_client(int max, int cur) = 0;
    // Callback for logrotate.
    virtual void on_logrotate() = 0;
};

class SrsServer : public ISrsCoroutineHandler, public ISrsResourceManager
{
private:
    ISrsHttpServeMux* http_api_mux;
    SrsHttpServer* http_server;
    // If reusing, HTTP API use the same port of HTTP server.
    bool reuse_api_over_server_;
    // If reusing, WebRTC TCP use the same port of HTTP server.
    bool reuse_rtc_over_server_;
private:
    SrsResourceManager* conn_manager;
    SrsCoroutine* trd_;
    SrsWaitGroup* wg_;
private:
    int pid_fd;
    // All listners, listener manager.
    std::vector<SrsListener*> listeners;
    // Signal manager which convert gignal to io message.
    SrsSignalManager* signal_manager;    
    // Handle in server cycle.
    ISrsServerCycle* handler;
    // User send the signal, convert to variable.
    bool signal_reload;
    bool signal_persistence_config;
    bool signal_gmc_stop;
    bool signal_fast_quit;
    bool signal_gracefully_quit;
    // Parent pid for asprocess.
    int ppid;
public:
    SrsServer();
    virtual ~SrsServer();
public:
    // Initialize server with callback handler ch.
    // @remark user must free the handler.
    virtual srs_error_t initialize(ISrsServerCycle* ch);
    virtual srs_error_t initialize_st();
    virtual srs_error_t initialize_signal();
    virtual srs_error_t listen();
    virtual srs_error_t register_signal();
    virtual srs_error_t http_handle();
public:
    virtual srs_error_t start(SrsWaitGroup* wg);
// interface ISrsCoroutineHandler
public:
    virtual srs_error_t cycle();
// server utilities.
public:
    // The callback for signal manager got a signal.
    // The signal manager convert signal to io message,
    // whatever, we will got the signo like the orignal signal(int signo) handler.
    // @param signo the signal number from user, where:
    //      SRS_SIGNAL_FAST_QUIT, the SIGTERM, do essential dispose then quit.
    //      SRS_SIGNAL_GRACEFULLY_QUIT, the SIGQUIT, do careful dispose then quit.
    //      SRS_SIGNAL_REOPEN_LOG, the SIGUSR1, reopen the log file.
    //      SRS_SIGNAL_RELOAD, the SIGHUP, reload the config.
    //      SRS_SIGNAL_PERSISTENCE_CONFIG, application level signal, persistence config to file.
    // @remark, for SIGINT:
    //       no gmc, fast quit, do essential dispose then quit.
    //       for gmc, set the variable signal_gmc_stop, the cycle will return and cleanup for gmc.
    // @remark, maybe the HTTP RAW API will trigger the on_signal() also.
    virtual void on_signal(int signo);
private:
    // The server thread main cycle,
    // update the global static data, for instance, the current time,
    // the cpu/mem/network statistic.
    virtual srs_error_t do_cycle();
private:
    // listen at specified protocol.
    // virtual srs_error_t listen_rtmp();
    virtual srs_error_t listen_http_api();
    virtual srs_error_t listen_https_api();
    // listen http/https proxy connection
    virtual srs_error_t listen_http_proxy();
    // Close the listeners for specified type,
    // Remove the listen object from manager.
    virtual void close_listeners(SrsListenerType type);    
// For internal only
public:
    // When listener got a fd, notice server to accept it.
    // @param type, the client type, used to create concrete connection,
    //       for instance RTMP connection to serve client.
    // @param stfd, the client fd in st boxed, the underlayer fd.
    virtual srs_error_t accept_client(SrsListenerType type, srs_netfd_t stfd);
private:
    virtual srs_error_t fd_to_resource(SrsListenerType type, srs_netfd_t& stfd, ISrsResource** pr);
// Interface ISrsResourceManager
public:
    // A callback for connection to remove itself.
    // When connection thread cycle terminated, callback this to delete connection.
    // @see SrsTcpConnection.on_thread_stop().
    virtual void remove(ISrsResource* c);
};

// The SRS server adapter, the master server.
class SrsServerAdapter : public ISrsHybridServer
{
private:
    SrsServer* srs;
public:
    SrsServerAdapter();
    virtual ~SrsServerAdapter();
public:
    virtual srs_error_t initialize();
    virtual srs_error_t run(SrsWaitGroup* wg);
    virtual void stop();
public:
    virtual SrsServer* instance();
};


#endif