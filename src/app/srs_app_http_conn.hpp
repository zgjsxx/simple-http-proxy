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

#include <unordered_map>
using std::unordered_map;
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
/*
    client ----------->proxy ----------->server
    client <-----------proxy <-----------server
*/
    // The manager object to manage the connection.
    ISrsResourceManager* manager;
    //client socket reader/writer
    ISrsProtocolReadWriter* clt_skt; //clt = client
    //client socket reader/writer
    ISrsProtocolReadWriter* svr_skt; //svr = server
    //client ssl 
    SrsSslConnection* clt_ssl;
    //server ssl
    SrsSslClient* svr_ssl;
    //client http connect message if it has
    SrsHttpMessage* client_connect_req;
    //client http message
    SrsHttpMessage* client_http_req;
    //server http message
    SrsHttpMessage* server_http_resp;
    string req_body;
    string resp_body;
    //http protocol parser
    SrsHttpParser* parser;
    //http server response protocol parser
    SrsHttpParser* server_parser;
    //coroutine 
    SrsCoroutine* trd;
    // The ip and port of client.
    std::string ip;
    int port;
    //check whether connection is a https request
    bool is_https;
public:
    SrsHttpxProxyConn(ISrsProtocolReadWriter* io, ISrsResourceManager* cm, ISrsHttpServeMux* m, std::string cip, int port);
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
public:
    virtual srs_error_t check_http_or_https();
    virtual srs_error_t process_http_connection();
    virtual srs_error_t process_https_connection();
    virtual int pass(ISrsProtocolReadWriter* in, ISrsProtocolReadWriter* out);
    virtual srs_error_t processHttpsTunnel();
public:
    virtual srs_error_t on_disconnect();
    virtual srs_error_t on_conn_done(srs_error_t r0);
public:
    //get url actegory of the url
    virtual srs_error_t detect_url_category();
public:
    virtual srs_error_t prepare403block();
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

class ResignEndpointCert
{
public:
    ResignEndpointCert();
    ResignEndpointCert(X509* x509, EVP_PKEY* key);
    ~ResignEndpointCert();
private:
    X509 *resign_x509;
	EVP_PKEY* resign_key;
public:
    virtual X509* get_resign_cert();
    virtual EVP_PKEY* get_resign_key();
};

class ResignEndpointCertMap
{
public:
    ResignEndpointCertMap();
    ~ResignEndpointCertMap();
public:
    void insert(string domain, ResignEndpointCert* cert);
    int count(string domain);
    ResignEndpointCert* get(string domain);
private:
    unordered_map<string, ResignEndpointCert*> m_domain_cert_map;
};
#endif