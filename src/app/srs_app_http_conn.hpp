#ifndef SRS_APP_HTTP_CONN_HPP
#define SRS_APP_HTTP_CONN_HPP

#include <srs_protocol_http_stack.hpp>
#include <srs_protocol_http_conn.hpp>
#include <srs_protocol_conn.hpp>
#include <srs_protocol_io.hpp>
#include <srs_app_st.hpp>
#include <srs_app_reload.hpp>
#include <srs_app_conn.hpp>
#include <srs_app_server.hpp>

class SrsHttpParser;

// The owner of HTTP connection.
class ISrsHttpConnOwner
{
public:
    ISrsHttpConnOwner();
    virtual ~ISrsHttpConnOwner();
public:
    // When start the coroutine to process connection.
    virtual srs_error_t on_start() = 0;
    // Handle the HTTP message r, which may be parsed partially.
    // For the static service or api, discard any body.
    // For the stream caster, for instance, http flv streaming, may discard the flv header or not.
    virtual srs_error_t on_http_message(ISrsHttpMessage* r, SrsHttpResponseWriter* w) = 0;
    // When message is processed, we may need to do more things.
    virtual srs_error_t on_message_done(ISrsHttpMessage* r, SrsHttpResponseWriter* w) = 0;
    // When connection is destroy, should use manager to dispose it.
    // The r0 is the original error, we will use the returned new error.
    virtual srs_error_t on_conn_done(srs_error_t r0) = 0;
};


// TODO: FIXME: Should rename to roundtrip or responder, not connection.
// The http connection which request the static or stream content.
class SrsHttpConn : public ISrsConnection, public ISrsCoroutineHandler, public ISrsStartable
    , public ISrsExpire
{
protected:
    SrsHttpParser* parser;
    ISrsHttpServeMux* http_mux;
    SrsHttpCorsMux* cors;
    ISrsHttpConnOwner* handler_;
protected:
    ISrsProtocolReadWriter* skt;
    // Each connection start a green thread,
    // when thread stop, the connection will be delete by server.
    SrsCoroutine* trd;
    // The ip and port of client.
    std::string ip;
    int port;
private:
    // The delta for statistic.
    // SrsNetworkDelta* delta_;
    // The create time in milliseconds.
    // for current connection to log self create time and calculate the living time.
    int64_t create_time;
public:
    SrsHttpConn(ISrsHttpConnOwner* handler, ISrsProtocolReadWriter* fd, ISrsHttpServeMux* m, std::string cip, int port);
    virtual ~SrsHttpConn();
// Interface ISrsResource.
public:
    virtual std::string desc();
// Interface ISrsStartable
public:
    // ISrsKbpsDelta* delta();
// Interface ISrsStartable
    virtual srs_error_t start();
// Interface ISrsOneCycleThreadHandler
public:
    virtual srs_error_t cycle();
private:
    virtual srs_error_t do_cycle();
    virtual srs_error_t process_requests();
    virtual srs_error_t process_request(ISrsHttpResponseWriter* w, ISrsHttpMessage* r, int rid); 
    // When the connection disconnect, call this method.
    // e.g. log msg of connection and report to other system.
    // @param request: request which is converted by the last http message.
    virtual srs_error_t on_disconnect();
public:
    // Whether the connection coroutine is error or terminated.
    virtual srs_error_t pull();
    // Whether enable the CORS(cross-domain).
    virtual srs_error_t set_crossdomain_enabled(bool v);
    // Whether enable the JSONP.
    virtual srs_error_t set_jsonp(bool v);
// Interface ISrsConnection.
public:
    virtual std::string remote_ip();   
    virtual const SrsContextId& get_id();
// Interface ISrsExpire.
public:
    virtual void expire();
};

//ISrsConnection inherit from ISrsResource
class SrsHttpxConn : public ISrsConnection, public ISrsStartable , public ISrsHttpConnOwner, public ISrsReloadHandler
{
private:
    // The manager object to manage the connection.
    ISrsResourceManager* manager;
    ISrsProtocolReadWriter* io_;
    SrsSslConnection* ssl;
    SrsHttpConn* conn;
    // We should never enable the stat, unless HTTP stream connection requires.
    bool enable_stat_;
public:
    SrsHttpxConn(bool https, ISrsResourceManager* cm, ISrsProtocolReadWriter* io, ISrsHttpServeMux* m, std::string cip, int port);
    virtual ~SrsHttpxConn();
public:
    // Require statistic about HTTP connection, for HTTP streaming clients only.
    void set_enable_stat(bool v);
// Interface ISrsHttpConnOwner.
public:
    virtual srs_error_t on_start();
    virtual srs_error_t on_http_message(ISrsHttpMessage* r, SrsHttpResponseWriter* w);
    virtual srs_error_t on_message_done(ISrsHttpMessage* r, SrsHttpResponseWriter* w);
    virtual srs_error_t on_conn_done(srs_error_t r0);
// Interface ISrsResource.
public:
    virtual std::string desc();
// Interface ISrsConnection.
public:
    virtual std::string remote_ip();
    virtual const SrsContextId& get_id();
// Interface ISrsStartable
public:
    virtual srs_error_t start();
// public:
//     ISrsKbpsDelta* delta();
};

class SrsHttpxProxyConn :  public ISrsCoroutineHandler, public ISrsConnection, public ISrsStartable//public ISrsConnection , public ISrsHttpConnOwner, public ISrsReloadHandler,
{
private:
    //client socket reader/writer
    ISrsProtocolReadWriter* skt;
    //http protocol parser
    SrsHttpParser* parser;
    //coroutine 
    SrsCoroutine* trd;
    // The ip and port of client.
    std::string ip;
    int port;
public:
    SrsHttpxProxyConn(ISrsProtocolReadWriter* io, ISrsHttpServeMux* m, std::string cip, int port);
    virtual ~SrsHttpxProxyConn();
public:
    virtual srs_error_t start();
// Interface ISrsOneCycleThreadHandler
public:
    virtual srs_error_t cycle();
private:
    virtual srs_error_t do_cycle();
    virtual srs_error_t process_requests();
public:
    virtual const SrsContextId& get_id();
    virtual std::string desc();
    virtual std::string remote_ip();
};

// The http server, use http stream or static server to serve requests.
class SrsHttpServer : public ISrsHttpServeMux
{
private:
    SrsServer* server;
public:
    SrsHttpServer(SrsServer* svr);
    virtual ~SrsHttpServer();
public:
    virtual srs_error_t initialize();
// Interface ISrsHttpServeMux
public:
    virtual srs_error_t handle(std::string pattern, ISrsHttpHandler* handler);
// Interface ISrsHttpHandler
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

#endif