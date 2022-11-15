#include <srs_app_http_client.hpp>
#include <srs_app_conn.hpp>
#include <srs_protocol_http_conn.hpp>
#include <srs_kernel_consts.hpp>

SrsHttpClient::SrsHttpClient()
{
    transport = NULL;
    ssl_transport = NULL;
    // kbps = new SrsNetworkKbps();
    parser = NULL;
    recv_timeout = timeout = SRS_UTIME_NO_TIMEOUT;
    port = 0;
}

SrsHttpClient::~SrsHttpClient()
{
    disconnect();

    // srs_freep(kbps);
    srs_freep(parser);
}

srs_error_t SrsHttpClient::initialize(string schema, string h, int p, srs_utime_t tm)
{
    srs_error_t err = srs_success;
    
    srs_freep(parser);
    parser = new SrsHttpParser();
    
    if ((err = parser->initialize(HTTP_RESPONSE)) != srs_success) {
        return srs_error_wrap(err, "http: init parser");
    }
    
    // Always disconnect the transport.
    schema_ = schema;
    host = h;
    port = p;
    recv_timeout = timeout = tm;
    disconnect();
    
    // ep used for host in header.
    string ep = host;
    if (port > 0 && port != SRS_CONSTS_HTTP_DEFAULT_PORT) {
        ep += ":" + srs_int2str(port);
    }
    
    // Set default value for headers.
    headers["Host"] = ep;
    headers["Connection"] = "Keep-Alive";
    headers["User-Agent"] = RTMP_SIG_SRS_SERVER;
    headers["Content-Type"] = "application/json";
    
    return err;
}

SrsHttpClient* SrsHttpClient::set_header(string k, string v)
{
    headers[k] = v;
    
    return this;
}

srs_error_t SrsHttpClient::post(string path, string req, ISrsHttpMessage** ppmsg)
{
    *ppmsg = NULL;
    
    srs_error_t err = srs_success;
    
    // always set the content length.
    headers["Content-Length"] = srs_int2str(req.length());
    
    if ((err = connect()) != srs_success) {
        return srs_error_wrap(err, "http: connect server");
    }
    
    if (path.size() == 0) {
        path = "/";
    }

    // send POST request to uri
    // POST %s HTTP/1.1\r\nHost: %s\r\nContent-Length: %d\r\n\r\n%s
    std::stringstream ss;
    ss << "POST " << path << " " << "HTTP/1.1" << SRS_HTTP_CRLF;
    for (map<string, string>::iterator it = headers.begin(); it != headers.end(); ++it) {
        string key = it->first;
        string value = it->second;
        ss << key << ": " << value << SRS_HTTP_CRLF;
    }
    ss << SRS_HTTP_CRLF << req;

    std::string data = ss.str();
    if ((err = writer()->write((void*)data.c_str(), data.length(), NULL)) != srs_success) {
        // Disconnect the transport when channel error, reconnect for next operation.
        disconnect();
        return srs_error_wrap(err, "http: write");
    }
    
    ISrsHttpMessage* msg = NULL;
    if ((err = parser->parse_message(reader(), &msg)) != srs_success) {
        return srs_error_wrap(err, "http: parse response");
    }
    srs_assert(msg);
    
    if (ppmsg) {
        *ppmsg = msg;
    } else {
        srs_freep(msg);
    }
    
    return err;
}

srs_error_t SrsHttpClient::get(string path, string req, ISrsHttpMessage** ppmsg)
{
    *ppmsg = NULL;
    
    srs_error_t err = srs_success;
    
    // always set the content length.
    headers["Content-Length"] = srs_int2str(req.length());
    
    if ((err = connect()) != srs_success) {
        return srs_error_wrap(err, "http: connect server");
    }
    
    // send POST request to uri
    // GET %s HTTP/1.1\r\nHost: %s\r\nContent-Length: %d\r\n\r\n%s
    std::stringstream ss;
    ss << "GET " << path << " " << "HTTP/1.1" << SRS_HTTP_CRLF;
    for (map<string, string>::iterator it = headers.begin(); it != headers.end(); ++it) {
        string key = it->first;
        string value = it->second;
        ss << key << ": " << value << SRS_HTTP_CRLF;
    }
    ss << SRS_HTTP_CRLF << req;
    
    std::string data = ss.str();
    if ((err = writer()->write((void*)data.c_str(), data.length(), NULL)) != srs_success) {
        // Disconnect the transport when channel error, reconnect for next operation.
        disconnect();
        return srs_error_wrap(err, "http: write");
    }
    
    ISrsHttpMessage* msg = NULL;
    if ((err = parser->parse_message(reader(), &msg)) != srs_success) {
        return srs_error_wrap(err, "http: parse response");
    }
    srs_assert(msg);
    
    if (ppmsg) {
        *ppmsg = msg;
    } else {
        srs_freep(msg);
    }
    
    return err;
}

void SrsHttpClient::set_recv_timeout(srs_utime_t tm)
{
    recv_timeout = tm;
}

// void SrsHttpClient::kbps_sample(const char* label, srs_utime_t age)
// {
//     kbps->sample();

//     int sr = kbps->get_send_kbps();
//     int sr30s = kbps->get_send_kbps_30s();
//     int sr5m = kbps->get_send_kbps_5m();
//     int rr = kbps->get_recv_kbps();
//     int rr30s = kbps->get_recv_kbps_30s();
//     int rr5m = kbps->get_recv_kbps_5m();

//     srs_trace("<- %s time=%" PRId64 ", okbps=%d,%d,%d, ikbps=%d,%d,%d", label, srsu2ms(age), sr, sr30s, sr5m, rr, rr30s, rr5m);
// }

void SrsHttpClient::disconnect()
{
    // kbps->set_io(NULL, NULL);
    srs_freep(ssl_transport);
    srs_freep(transport);
}

srs_error_t SrsHttpClient::connect()
{
    srs_error_t err = srs_success;
    
    // When transport connected, ignore.
    if (transport) {
        return err;
    }
    
    transport = new SrsTcpClient(host, port, timeout);
    if ((err = transport->connect()) != srs_success) {
        disconnect();
        return srs_error_wrap(err, "http: tcp connect %s %s:%d to=%dms, rto=%dms",
            schema_.c_str(), host.c_str(), port, srsu2msi(timeout), srsu2msi(recv_timeout));
    }
    
    // Set the recv/send timeout in srs_utime_t.
    transport->set_recv_timeout(recv_timeout);
    transport->set_send_timeout(timeout);

    // kbps->set_io(transport, transport);
    
    if (schema_ != "https") {
        return err;
    }

#if !defined(SRS_HTTPS)
    return srs_error_new(ERROR_HTTPS_NOT_SUPPORTED, "should configure with --https=on");
#else
    srs_assert(!ssl_transport);
    ssl_transport = new SrsSslClient(transport);

    srs_utime_t starttime = srs_update_system_time();

    if ((err = ssl_transport->handshake()) != srs_success) {
        disconnect();
        return srs_error_wrap(err, "http: ssl connect %s %s:%d to=%dms, rto=%dms",
            schema_.c_str(), host.c_str(), port, srsu2msi(timeout), srsu2msi(recv_timeout));
    }

    int cost = srsu2msi(srs_update_system_time() - starttime);
    srs_trace("https: connected to %s://%s:%d, cost=%dms", schema_.c_str(), host.c_str(), port, cost);

    return err;
#endif
}

ISrsStreamWriter* SrsHttpClient::writer()
{
    if (ssl_transport) {
        return ssl_transport;
    }
    return transport;
}

ISrsReader* SrsHttpClient::reader()
{
    if (ssl_transport) {
        return ssl_transport;
    }
    return transport;
}

