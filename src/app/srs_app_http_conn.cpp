//
// Copyright (c) 2013-2022 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//

#include <srs_app_http_conn.hpp>
#include <srs_kernel_log.hpp>
#include <srs_core_platform.hpp>
#include <srs_core_auto_free.hpp>
#include <srs_app_config.hpp>
#include <srs_kernel_consts.hpp>

extern ISrsContext* _srs_context;
extern SrsConfig* _srs_config;
ISrsHttpConnOwner::ISrsHttpConnOwner()
{
}

ISrsHttpConnOwner::~ISrsHttpConnOwner()
{
}

SrsHttpConn::SrsHttpConn(ISrsHttpConnOwner* handler, ISrsProtocolReadWriter* fd, ISrsHttpServeMux* m, string cip, int cport)
{
    parser = new SrsHttpParser();
    cors = new SrsHttpCorsMux();
    http_mux = m;
    handler_ = handler;

    skt = fd;
    ip = cip;
    port = cport;
    create_time = srsu2ms(srs_get_system_time());
    // delta_ = new SrsNetworkDelta();
    // delta_->set_io(skt, skt);
    trd = new SrsSTCoroutine("http", this, _srs_context->get_id());
}

SrsHttpConn::~SrsHttpConn()
{
    trd->interrupt();
    srs_trace("free trd");
    srs_freep(trd);

    srs_freep(parser);
    srs_freep(cors);

    // srs_freep(delta_);
}

std::string SrsHttpConn::desc()
{
    return "HttpConn";
}

srs_error_t SrsHttpConn::set_jsonp(bool v)
{
    parser->set_jsonp(v);
    return srs_success;
}

srs_error_t SrsHttpConn::cycle()
{
    srs_error_t err = do_cycle();

    // Notify handler to handle it.
    // @remark The error may be transformed by handler.
    err = handler_->on_conn_done(err);
   // success.
    if (err == srs_success) {
        srs_trace("client finished.");
        return err;
    }

    // It maybe success with message.
    if (srs_error_code(err) == ERROR_SUCCESS) {
        srs_trace("client finished%s.", srs_error_summary(err).c_str());
        srs_freep(err);
        return err;
    }

    // client close peer.
    // TODO: FIXME: Only reset the error when client closed it.
    if (srs_is_client_gracefully_close(err)) {
        srs_warn("client disconnect peer. ret=%d", srs_error_code(err));
    } else if (srs_is_server_gracefully_close(err)) {
        srs_warn("server disconnect. ret=%d", srs_error_code(err));
    } else {
        srs_error("serve error %s", srs_error_desc(err).c_str());
    }

    srs_freep(err);
    return srs_success;
}

srs_error_t SrsHttpConn::do_cycle()
{
    srs_error_t err = srs_success;
    
    // set the recv timeout, for some clients never disconnect the connection.
    // @see https://github.com/ossrs/srs/issues/398
    skt->set_recv_timeout(SRS_HTTP_RECV_TIMEOUT);

    // initialize parser
    if ((err = parser->initialize(HTTP_REQUEST)) != srs_success) {
        return srs_error_wrap(err, "init parser for %s", ip.c_str());
    }

    // Notify the handler that we are starting to process the connection.
    if ((err = handler_->on_start()) != srs_success) {
        return srs_error_wrap(err, "start");
    }

    // process all http messages.
    err = process_requests();
    
    srs_error_t r0 = srs_success;
    if ((r0 = on_disconnect()) != srs_success) {
        err = srs_error_wrap(err, "on disconnect %s", srs_error_desc(r0).c_str());
        srs_freep(r0);
    }
}

srs_error_t SrsHttpConn::on_disconnect()
{
    // TODO: FIXME: Implements it.
    return srs_success;
}

srs_error_t SrsHttpConn::process_requests()
{
    srs_error_t err = srs_success;
    for (int req_id = 0; ; req_id++) {
        if ((err = trd->pull()) != srs_success) {
            return srs_error_wrap(err, "pull");
        }

        // get a http message
        ISrsHttpMessage* req = NULL;
        if ((err = parser->parse_message(skt, &req)) != srs_success) {
            return srs_error_wrap(err, "parse message");
        }

        // if SUCCESS, always NOT-NULL.
        // always free it in this scope.
        srs_assert(req);
        SrsAutoFree(ISrsHttpMessage, req);

        // Attach owner connection to message.
        SrsHttpMessage* hreq = (SrsHttpMessage*)req;
        hreq->set_connection(this);

        // may should discard the body.
        SrsHttpResponseWriter writer(skt);
        if ((err = handler_->on_http_message(req, &writer)) != srs_success) {
            return srs_error_wrap(err, "on http message");
        }

        // ok, handle http request.
        if ((err = process_request(&writer, req, req_id)) != srs_success) {
            return srs_error_wrap(err, "process request=%d", req_id);
        }

        // After the request is processed.
        if ((err = handler_->on_message_done(req, &writer)) != srs_success) {
            return srs_error_wrap(err, "on message done");
        }

        // donot keep alive, disconnect it.
        // @see https://github.com/ossrs/srs/issues/399
        if (!req->is_keep_alive()) {
            break;
        }
    }
}

srs_error_t SrsHttpConn::process_request(ISrsHttpResponseWriter* w, ISrsHttpMessage* r, int rid)
{
    srs_error_t err = srs_success;
    
    // srs_trace("HTTP #%d %s:%d %s %s, content-length=%" PRId64 "", rid, ip.c_str(), port,
    //     r->method_str().c_str(), r->url().c_str(), r->content_length());
    
    // use cors server mux to serve http request, which will proxy to http_remux.
    if ((err = cors->serve_http(w, r)) != srs_success) {
        return srs_error_wrap(err, "mux serve");
    }
}

srs_error_t SrsHttpConn::pull()
{
    return trd->pull();
}

srs_error_t SrsHttpConn::set_crossdomain_enabled(bool v)
{
    srs_error_t err = srs_success;

    // initialize the cors, which will proxy to mux.
    if ((err = cors->initialize(http_mux, v)) != srs_success) {
        return srs_error_wrap(err, "init cors");
    }

    return err;
}


srs_error_t SrsHttpConn::start()
{
    srs_error_t err = srs_success;

    srs_trace("trd->start");
    if ((err = trd->start()) != srs_success) {
        return srs_error_wrap(err, "coroutine");
    }

    return err;
}

void SrsHttpConn::expire()
{
    trd->interrupt();
}

string SrsHttpConn::remote_ip()
{
    return ip;
}

const SrsContextId& SrsHttpConn::get_id()
{
    return trd->cid();
}

SrsHttpxConn::SrsHttpxConn(bool https, ISrsResourceManager* cm, ISrsProtocolReadWriter* io, ISrsHttpServeMux* m, string cip, int port)
{
    // Create a identify for this client.
    _srs_context->set_id(_srs_context->generate_id());

    io_ = io;
    manager = cm;
    enable_stat_ = false;

    if (https) {
        ssl = new SrsSslConnection(io_);
        conn = new SrsHttpConn(this, ssl, m, cip, port);
    } else {
        ssl = NULL;
        conn = new SrsHttpConn(this, io_, m, cip, port);
    }

    _srs_config->subscribe(this);
}

SrsHttpxConn::~SrsHttpxConn()
{
    _srs_config->unsubscribe(this);

    srs_freep(conn);
    srs_freep(ssl);

    srs_trace("free io");
    srs_freep(io_);
}

void SrsHttpxConn::set_enable_stat(bool v)
{
    enable_stat_ = v;
}

srs_error_t SrsHttpxConn::on_start()
{
    srs_error_t err = srs_success;

    // Enable JSONP for HTTP API.
    if ((err = conn->set_jsonp(true)) != srs_success) {
        return srs_error_wrap(err, "set jsonp");
    }
    // Do SSL handshake if HTTPS.
    if (ssl)  {
        srs_utime_t starttime = srs_update_system_time();
        string crt_file = _srs_config->get_https_stream_ssl_cert();
        string key_file = _srs_config->get_https_stream_ssl_key();
        if ((err = ssl->handshake(key_file, crt_file)) != srs_success) {
            return srs_error_wrap(err, "handshake");
        }

        int cost = srsu2msi(srs_update_system_time() - starttime);
        srs_trace("https: stream server done, use key %s and cert %s, cost=%dms",
            key_file.c_str(), crt_file.c_str(), cost);
    }

    return err;
}

srs_error_t SrsHttpxConn::on_http_message(ISrsHttpMessage* r, SrsHttpResponseWriter* w)
{
    srs_error_t err = srs_success;

    // After parsed the message, set the schema to https.
    if (ssl) {
        SrsHttpMessage* hm = dynamic_cast<SrsHttpMessage*>(r);
        hm->set_https(true);
    }

    // For each session, we use short-term HTTP connection.
    SrsHttpHeader* hdr = w->header();
    // hdr->set("Connection", "Close");
    
    return err;
}

srs_error_t SrsHttpxConn::on_message_done(ISrsHttpMessage* r, SrsHttpResponseWriter* w)
{
    return srs_success;
}

srs_error_t SrsHttpxConn::on_conn_done(srs_error_t r0)
{
    // Because we use manager to manage this object,
    // not the http connection object, so we must remove it here.
    manager->remove(this);

    // For HTTP-API timeout, we think it's done successfully,
    // because there may be no request or response for HTTP-API.
    if (srs_error_code(r0) == ERROR_SOCKET_TIMEOUT) {
        srs_freep(r0);
        return srs_success;
    }

    return r0;
}

std::string SrsHttpxConn::desc()
{
    if (ssl) {
        return "HttpsConn";
    }
    return "HttpConn";
}

std::string SrsHttpxConn::remote_ip()
{
    return conn->remote_ip();
}

const SrsContextId& SrsHttpxConn::get_id()
{
    return conn->get_id();
}

srs_error_t SrsHttpxConn::start()
{
    srs_error_t err = srs_success;

    bool v = _srs_config->get_http_stream_crossdomain();
    if ((err = conn->set_crossdomain_enabled(v)) != srs_success) {
        return srs_error_wrap(err, "set cors=%d", v);
    }

    return conn->start();
}

SrsHttpxProxyConn::SrsHttpxProxyConn(ISrsProtocolReadWriter* io, ISrsHttpServeMux* m, std::string cip, int port)
{
    parser = new SrsHttpParser();
    server_parser = new SrsHttpParser();
    ip = cip;
    port = port;
    skt = io;
    trd = new SrsSTCoroutine("httpProxy", this, _srs_context->get_id());
}

SrsHttpxProxyConn::~SrsHttpxProxyConn()
{
    trd->interrupt();
    srs_freep(trd);
    srs_freep(parser);
    srs_freep(server_parser);
}

srs_error_t SrsHttpxProxyConn::start()
{
    srs_error_t err = srs_success;

    if ((err = trd->start()) != srs_success) {
        return srs_error_wrap(err, "coroutine");
    }

    return err;
}

srs_error_t SrsHttpxProxyConn::cycle()
{
    srs_error_t err = do_cycle();
    return err;
}

srs_error_t SrsHttpxProxyConn::do_cycle()
{
    srs_error_t err = srs_success;
    
    // // set the recv timeout, for some clients never disconnect the connection.
    // // @see https://github.com/ossrs/srs/issues/398
    // skt->set_recv_timeout(SRS_HTTP_RECV_TIMEOUT);

    // initialize parser
    if ((err = parser->initialize(HTTP_REQUEST)) != srs_success) {
        return srs_error_wrap(err, "init parser for %s", ip.c_str());
    }

    // process all http messages.
    err = process_requests();
    
    // srs_error_t r0 = srs_success;
    // if ((r0 = on_disconnect()) != srs_success) {
    //     err = srs_error_wrap(err, "on disconnect %s", srs_error_desc(r0).c_str());
    //     srs_freep(r0);
    // } 
}

srs_error_t SrsHttpxProxyConn::process_requests()
{
    srs_error_t err = srs_success;
    for (int req_id = 0; ; req_id++) {
        if ((err = trd->pull()) != srs_success) {
            return srs_error_wrap(err, "pull");
        }

        // get a http message
        // current, we are sure to get http header, body is not sure
        ISrsHttpMessage* req = NULL;
        if ((err = parser->parse_message(skt, &req)) != srs_success) {
            return srs_error_wrap(err, "parse message");
        }

        // if SUCCESS, always NOT-NULL.
        // always free it in this scope.
        srs_assert(req);
        SrsAutoFree(ISrsHttpMessage, req);

        // Attach owner connection to message.
        SrsHttpMessage* hreq = (SrsHttpMessage*)req;
        hreq->set_connection(this);
        string req_body = "";
        hreq->body_read_all(req_body);

        srs_trace("dest_domain is %s, dest_port is %d", hreq->get_dest_domain().c_str(), hreq->get_dest_port());
        svr_skt = new SrsTcpClient(hreq->get_dest_domain(), hreq->get_dest_port(), SRS_UTIME_NO_TIMEOUT);

        SrsTcpClient* server_skt = (SrsTcpClient*)svr_skt;
        server_skt->connect();
        server_skt->write(const_cast<char*>(hreq->get_raw_header().c_str()), hreq->get_raw_header().size(), NULL);
        server_skt->write(const_cast<char*>(req_body.c_str()), req_body.size(), NULL);

        // get a http response
        // current, we are sure to get http header, body is not sure
        if ((err = server_parser->initialize(HTTP_RESPONSE)) != srs_success) {
            return srs_error_wrap(err, "init parser for %s", ip.c_str());
        }

        ISrsHttpMessage* server_resp = NULL;
        if ((err = server_parser->parse_message(svr_skt, &server_resp)) != srs_success) {
            return srs_error_wrap(err, "parse message");
        }

        // Attach owner connection to message.
        SrsHttpMessage* hresp = (SrsHttpMessage*)server_resp;
        skt->write(const_cast<char*>(hresp->get_raw_header().c_str()), hresp->get_raw_header().size(), NULL);
        
        string resp_body = "";
        hresp->body_read_all(resp_body);
        skt->write(const_cast<char*>(resp_body.c_str()), resp_body.size(), NULL);


    //     // may should discard the body.
    //     SrsHttpResponseWriter writer(skt);
    //     if ((err = handler_->on_http_message(req, &writer)) != srs_success) {
    //         return srs_error_wrap(err, "on http message");
    //     }

    //     // ok, handle http request.
    //     if ((err = process_request(&writer, req, req_id)) != srs_success) {
    //         return srs_error_wrap(err, "process request=%d", req_id);
    //     }

    //     // After the request is processed.
    //     if ((err = handler_->on_message_done(req, &writer)) != srs_success) {
    //         return srs_error_wrap(err, "on message done");
    //     }

    //     // donot keep alive, disconnect it.
    //     // @see https://github.com/ossrs/srs/issues/399
    //     if (!req->is_keep_alive()) {
    //         break;
    //     }
    }
}

string SrsHttpxProxyConn::remote_ip()
{
    return ip;
}

const SrsContextId& SrsHttpxProxyConn::get_id()
{
    return trd->cid();
}

std::string SrsHttpxProxyConn::desc()
{
    return "HttpProxy";
}

srs_error_t SrsHttpServer::initialize()
{
    srs_error_t err = srs_success;
    srs_trace("srs_success");
    // // for SRS go-sharp to detect the status of HTTP server of SRS HTTP FLV Cluster.
    // if ((err = http_static->mux.handle("/api/v1/versions", new SrsGoApiVersion())) != srs_success) {
    //     return srs_error_wrap(err, "handle versions");
    // }
    
    // if ((err = http_stream->initialize()) != srs_success) {
    //     return srs_error_wrap(err, "http stream");
    // }
    
    // if ((err = http_static->initialize()) != srs_success) {
    //     return srs_error_wrap(err, "http static");
    // }
    
    return err;
}

srs_error_t SrsHttpServer::handle(std::string pattern, ISrsHttpHandler* handler)
{
    // return http_static->mux.handle(pattern, handler);
}

srs_error_t SrsHttpServer::serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r)
{
    srs_error_t err = srs_success;
}

SrsHttpServer::SrsHttpServer(SrsServer* svr)
{
    server = svr;
    // http_stream = new SrsHttpStreamServer(svr);
    // http_static = new SrsHttpStaticServer(svr);
}

SrsHttpServer::~SrsHttpServer()
{
    // srs_freep(http_stream);
    // srs_freep(http_static);
}