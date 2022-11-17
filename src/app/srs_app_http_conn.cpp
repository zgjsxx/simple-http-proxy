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
#include <srs_app_policy.hpp>
#include <srs_kernel_file.hpp>
#include <srs_app_http_client.hpp>
#include <srs_protocol_json.hpp>
#include <srs_app_access_log.hpp>
#include <poll.h>

extern ISrsContext* _srs_context;
extern SrsConfig* _srs_config;
extern SrsPolicy* _srs_policy;
extern SrsNotification* _srs_notification;
extern SrsAccessLog* _srs_access_log;

EVP_PKEY *ca_key = NULL;

static void prepareResignCA()
{
	ca_key = EVP_PKEY_new();
	RSA *rsa = RSA_new();

	FILE *fp;
	if ((fp = fopen("conf/cert/ca-key.pem", "r")) == NULL)
	{
		srs_error("load private.key failed");
	}
	PEM_read_RSAPrivateKey(fp, &rsa, NULL, NULL);

	if ((fp = fopen("conf/cert/ca-cert.pem", "r")) == NULL)
	{
		srs_error("laod public.key failed");
	}
	PEM_read_RSAPublicKey(fp, &rsa, NULL, NULL);
	EVP_PKEY_assign_RSA(ca_key, rsa);
}

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

SrsHttpxProxyConn::SrsHttpxProxyConn(ISrsProtocolReadWriter* io, ISrsResourceManager* cm, ISrsHttpServeMux* m, std::string cip, int port)
{
    parser = new SrsHttpParser();
    server_parser = new SrsHttpParser();
    ip = cip;
    port = port;
    clt_skt = io;
    manager = cm;
    trd = new SrsSTCoroutine("httpProxy", this, _srs_context->generate_id());
    
    svr_skt = NULL;
    clt_ssl = NULL;
    svr_ssl = NULL;

}

SrsHttpxProxyConn::~SrsHttpxProxyConn()
{
    trd->interrupt();
    srs_freep(trd);
    srs_freep(parser);
    srs_freep(server_parser);
    srs_freep(clt_skt);

    if(svr_skt)
    {
        srs_freep(svr_skt);
    }
    if(clt_ssl)
    {
        srs_freep(clt_ssl);
    }
    if(svr_ssl)
    {
        srs_freep(svr_ssl);
    }

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
    // Notify handler to handle it.
    // @remark The error may be transformed by handler.

    err = on_conn_done(err);
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

    if(err != srs_success)
    {
        srs_error("do_cycle error %s.", srs_error_desc(err).c_str());
    }

    srs_freep(err);
    return srs_success;
}

srs_error_t SrsHttpxProxyConn::on_conn_done(srs_error_t r0)
{
    // Because we use manager to manage this object,
    // not the http connection object, so we must remove it here.
    manager->remove(this);

    // For HTTP-API timeout, we think it's done successfully,
    // because there may be no request or response for HTTP-API.
    if (srs_error_code(r0) == ERROR_SOCKET_TIMEOUT) {
        srs_trace("socket is timeout");
        srs_freep(r0);
        return srs_success;
    }
    
    return r0;
}

srs_error_t SrsHttpxProxyConn::do_cycle()
{
    srs_error_t err = srs_success;
    
    // // set the recv timeout, for some clients never disconnect the connection.
    // // @see https://github.com/ossrs/srs/issues/398
    clt_skt->set_recv_timeout(SRS_HTTP_RECV_TIMEOUT);

    // initialize parser
    if ((err = parser->initialize(HTTP_REQUEST)) != srs_success) {
        return srs_error_wrap(err, "init parser for %s", ip.c_str());
    }

    // process all http messages.
    err = process_requests();
    
    if(err != srs_success)
    {
        return err;
    }

    srs_error_t r0 = srs_success;
    if ((r0 = on_disconnect()) != srs_success) {
        err = srs_error_wrap(err, "on disconnect %s", srs_error_desc(r0).c_str());
        srs_freep(r0);
    } 
}

srs_error_t SrsHttpxProxyConn::process_requests()
{
    srs_error_t err = srs_success;
    if ((err = trd->pull()) != srs_success) {
        return srs_error_wrap(err, "pull");
    }

    // get a http message from client
    // current, we are sure to get http header, body is not sure
    SrsTcpConnection* client_tcp_clt = (SrsTcpConnection*)clt_skt;
    _srs_context->set_client_fd(client_tcp_clt->get_fd());

    ISrsHttpMessage* req = NULL;
    if ((err = parser->parse_message(clt_skt, &req)) != srs_success) {
        return srs_error_wrap(err, "parse message");
    }

    client_http_req = (SrsHttpMessage*)req;
    client_http_req->set_connection(this);
    // if SUCCESS, always NOT-NULL.
    // always free it in this scope.
    srs_assert(req);

    // Attach owner connection to message.
    srs_trace("dest_domain is %s, dest_port is %d", client_http_req->get_dest_domain().c_str(), client_http_req->get_dest_port());
    if(client_http_req->is_http_connect())
    {
        srs_trace("https connection is comming");
        err = process_https_connection();
        srs_trace("https connection is done");
        return err;
    }
    else
    {
        srs_trace("http connection is comming");
        err = process_http_connection();
        return err;
    }
}

srs_error_t SrsHttpxProxyConn::prepare403block()
{
    srs_error_t err;
    server_http_resp = new SrsHttpMessage();
    server_http_resp->set_basic(HTTP_RESPONSE, 0, 403, 0);
    SrsHttpHeader* block_header = server_http_resp->header();
    block_header->set_content_type("text/html");
    block_header->set("Connection", "close");

    resp_body = _srs_notification->getNotification(client_http_req->get_dest_domain(), remote_ip());
    block_header->set_content_length(resp_body.size());
    server_http_resp->restore_http_header();
    return err;
}

srs_error_t SrsHttpxProxyConn::process_http_connection()
{
    srs_error_t err = srs_success;
    for (int req_id = 0; ; req_id++) {
        ISrsHttpMessage* req = NULL;
        if(client_http_req == NULL)
        {
            if ((err = parser->parse_message(clt_skt, &req)) != srs_success) {
                return srs_error_wrap(err, "parse message");
            }
            client_http_req = (SrsHttpMessage*)req;
        }
        SrsAutoFree(ISrsHttpMessage, req);
        
        // beta
        // detect_url_category();

        if(_srs_policy->match_black_list(client_http_req->get_dest_domain()))
        {
            prepare403block();
            clt_skt->write(const_cast<char*>(server_http_resp->get_raw_header().c_str()), server_http_resp->get_raw_header().size(), NULL);
            clt_skt->write(const_cast<char*>(resp_body.c_str()), resp_body.size(), NULL);
            return err;
        }

        //if configure the next hip, forward traffic to next hip
        //client -> proxy ->next hip -> ... -> server
        //client <- proxy <-next hip <- ... <- server
        if(_srs_config->get_next_hip_proxy_enabled())
        {
            svr_skt = new SrsTcpClient(_srs_config->get_next_hip_proxy_ip(), atoi(_srs_config->get_next_hip_proxy_port().c_str()), SRS_UTIME_SECONDS * 5);
        }

        //send request to server
        if(svr_skt == NULL)
        {
            svr_skt = new SrsTcpClient(client_http_req->get_dest_domain(), client_http_req->get_dest_port(), SRS_UTIME_NO_TIMEOUT);
        }
        SrsTcpClient* server_skt = (SrsTcpClient*)svr_skt;

        server_skt->set_recv_timeout(SRS_HTTP_RECV_TIMEOUT);
        if((err = server_skt->connect()) != srs_success)
        {
            srs_trace("err = %d" , err == srs_success);
            return err;
        }
        _srs_context->set_server_fd(server_skt->get_fd());
        //forward client req header to server
        server_skt->write(const_cast<char*>(client_http_req->get_raw_header().c_str()), client_http_req->get_raw_header().size(), NULL);

        //check whether request has body
        if(client_http_req->is_chunked() || client_http_req->content_length() > 0)
        {
            client_http_req->body_read_all(req_body);
        }
        srs_trace("client req is chunked: %d, content_length: %d", client_http_req->is_chunked(), client_http_req->content_length());

        if(client_http_req->is_chunked())
        {
            char temp[32];
            snprintf(temp, sizeof(temp), "%x", req_body.size());
            srs_trace("write chunk data to server, chunk size is %s", temp);

            server_skt->write(temp, strlen(temp), NULL);
            server_skt->write(const_cast<char*>("\r\n"), 2, NULL);
            server_skt->write(const_cast<char*>(req_body.c_str()), req_body.size(), NULL);
            server_skt->write(const_cast<char*>("\r\n"), 2, NULL);
            server_skt->write(const_cast<char*>("0\r\n\r\n"), 5, NULL);
        }
        else if(client_http_req->content_length() > 0)
        {
            srs_trace("req_body is %d", req_body.size());
            server_skt->write(const_cast<char*>(req_body.c_str()), req_body.size(), NULL);
        }

        // get response from server
        // current, we are sure to get http header, body is not sure
        if ((err = server_parser->initialize(HTTP_RESPONSE)) != srs_success) {
            srs_trace("server_parser->initialize");
            return srs_error_wrap(err, "init parser for %s", ip.c_str());
        }

        ISrsHttpMessage* server_resp = NULL;
        if ((err = server_parser->parse_message(svr_skt, &server_resp)) != srs_success) {
            return srs_error_wrap(err, "parse message");
        }

        SrsAutoFree(ISrsHttpMessage, server_resp);
        // send response to client
        server_http_resp = (SrsHttpMessage*)server_resp;
        clt_skt->write(const_cast<char*>(server_http_resp->get_raw_header().c_str()), server_http_resp->get_raw_header().size(), NULL);

        if(server_http_resp->is_chunked() || server_http_resp->content_length() > 0)
        {
            while(1)
            {
                resp_body = "";
                srs_trace("start to read body");
                int finish = 0;
                err = server_http_resp->body_read_part(resp_body, 4096, finish);
                srs_trace("server req is chunked: %d, content_length: %d", server_http_resp->is_chunked(), server_http_resp->content_length());

                if(server_http_resp->is_chunked())
                {
                    char temp[32];
                    snprintf(temp, sizeof(temp), "%x", resp_body.size());
                    srs_trace("write chunk data to client, chunk size is %s", temp);

                    clt_skt->write(temp, strlen(temp), NULL);
                    clt_skt->write(const_cast<char*>("\r\n"), 2, NULL);
                    clt_skt->write(const_cast<char*>(resp_body.c_str()), resp_body.size(), NULL);
                    clt_skt->write(const_cast<char*>("\r\n"), 2, NULL);
                }
                else if(server_http_resp->content_length() > 0)
                {
                    srs_trace("resp_body is %d", resp_body.size());
                    clt_skt->write(const_cast<char*>(resp_body.c_str()), resp_body.size(), NULL);
                }

                if(finish)
                {
                    srs_trace("meet eof");
                    clt_skt->write(const_cast<char*>("0\r\n\r\n"), 5, NULL);
                    break;
                }
            }
        }   

        // if(server_http_resp->is_chunked() || server_http_resp->content_length() > 0)
        // {
        //     server_http_resp->body_read_all(resp_body);
        // }

        // srs_trace("server resp is chunked: %d, content_length: %d", server_http_resp->is_chunked(), server_http_resp->content_length());
        // if(server_http_resp->is_chunked())
        // {
        //     char temp[32];
        //     snprintf(temp, sizeof(temp), "%x", resp_body.size());
        //     srs_trace("write chunk data to client, chunk size is %s", temp);

        //     clt_skt->write(temp, strlen(temp), NULL);
        //     clt_skt->write(const_cast<char*>("\r\n"), 2, NULL);
        //     clt_skt->write(const_cast<char*>(resp_body.c_str()), resp_body.size(), NULL);
        //     clt_skt->write(const_cast<char*>("\r\n"), 2, NULL);
        //     clt_skt->write(const_cast<char*>("0\r\n\r\n"), 5, NULL);
        // }
        // else if(server_http_resp->content_length() > 0)
        // {
        //     srs_trace("write resp_body to client, size is %d", resp_body.size());
        //     clt_skt->write(const_cast<char*>(resp_body.c_str()), resp_body.size(), NULL);
        // }

        // donot keep alive, disconnect it.
        if (!client_http_req->is_keep_alive() || !server_http_resp->is_keep_alive()) {
            srs_trace("not keep-alive connection, close it now");
            break;
        }

        if(server_http_resp->status_code() == 101)
        {
            //web socket tracffic, tunnel it
            srs_trace("web socket traffic, tunnel it");
            processHttpsTunnel();
        }

        SrsAccessLogInfo* log_info = new SrsAccessLogInfo();
        SrsAutoFree(SrsAccessLogInfo, log_info);
        log_info->domain = client_http_req->get_dest_domain();
        log_info->client_ip = remote_ip();
        log_info->status_code = server_http_resp->status_code();
        _srs_access_log->write_access_log(log_info);

        resp_body = "";
        req_body = "";
        client_http_req = NULL;
        server_http_resp = NULL;
    }
    return err;
}

srs_error_t SrsHttpxProxyConn::process_https_connection()
{
    srs_error_t err = srs_success;
    if(_srs_config->get_next_hip_proxy_enabled())
    {
        //if configure the next hip, forward traffic to next hip
        //client -> proxy ->next hip -> ... -> server
        //client <- proxy <-next hip <- ... <- server        
        svr_skt = new SrsTcpClient(_srs_config->get_next_hip_proxy_ip(), atoi(_srs_config->get_next_hip_proxy_port().c_str()), SRS_UTIME_SECONDS * 5);
        SrsTcpClient* server_skt = (SrsTcpClient*)svr_skt;
        server_skt->set_recv_timeout(SRS_HTTP_RECV_TIMEOUT);
        if((err = server_skt->connect()) != srs_success)
        {
            srs_trace("err = %d" , err == srs_success);
            return err;
        }
        _srs_context->set_server_fd(server_skt->get_fd());
        server_skt->write(const_cast<char*>(client_http_req->get_raw_header().c_str()), client_http_req->get_raw_header().size(), NULL);
        //receive 200 connection established
        if ((err = server_parser->initialize(HTTP_RESPONSE)) != srs_success) {
            return srs_error_wrap(err, "init parser for %s", ip.c_str());
        }        
        ISrsHttpMessage* resp = NULL;
        if ((err = server_parser->parse_message(server_skt, &resp)) != srs_success) {
            return srs_error_wrap(err, "parse message");
        }
        SrsAutoFree(ISrsHttpMessage, resp);
    }

    // no next hip, connect directly, no need to foward connect request
    if(svr_skt == NULL)
    {
        svr_skt = new SrsTcpClient(client_http_req->get_dest_domain(), client_http_req->get_dest_port(), SRS_UTIME_SECONDS * 5);
        SrsTcpClient* server_skt = (SrsTcpClient*)svr_skt;

        server_skt->set_recv_timeout(SRS_HTTP_RECV_TIMEOUT);
        if((err = server_skt->connect()) != srs_success)
        {
            srs_trace("err = %d" , err == srs_success);
            return err;
        }
        _srs_context->set_server_fd(server_skt->get_fd());
    }

    //process_https_tunnel
    if(!_srs_policy->is_https_descrypt_enable() || _srs_policy->match_tunnel_domain_list(client_http_req->get_dest_domain()))
    {
        //connection to server established 
        //prepare 200 to client
        string res = "HTTP/1.1 200 Connection Established\r\n\r\n";
        clt_skt->write(const_cast<char*>(res.c_str()), res.size(), NULL);
        srs_trace("write HTTP 200 connection to client");
        processHttpsTunnel();
        return err;
    }

    svr_ssl = new SrsSslClient((SrsTcpClient*)svr_skt);
    svr_ssl->set_SNI(client_http_req->get_dest_domain());
    if((err = svr_ssl->handshake()) != srs_success)
    {
        srs_trace("server hadnshake failed");
        return err;
    }

	X509 *fake_x509 = X509_new();
	EVP_PKEY* server_key = EVP_PKEY_new();
    if(ca_key == NULL)
    {
        prepareResignCA();
    }
	
    svr_ssl->prepare_resign_endpoint(fake_x509, server_key);

    //connection to server established 
    //prepare 200 to client
    string res = "HTTP/1.1 200 Connection Established\r\n\r\n";
    clt_skt->write(const_cast<char*>(res.c_str()), res.size(), NULL);
    srs_trace("write HTTP 200 connection to client");

    clt_ssl = new SrsSslConnection(clt_skt);
    clt_ssl->handshake(fake_x509, server_key);

    //free the http connect msg
    srs_freep(client_http_req);

    for (int req_id = 0; ; req_id++) {
        // get a http message from client
        // current, we are sure to get http header, body is not sure
        ISrsHttpMessage* req = NULL;
        if ((err = parser->parse_message(clt_ssl, &req)) != srs_success) {
            return srs_error_wrap(err, "parse message");
        }
        SrsAutoFree(ISrsHttpMessage, req);

        client_http_req = (SrsHttpMessage*)req;
        client_http_req->set_connection(this);

        // beta
        // detect_url_category();

        if(_srs_policy->match_black_list(client_http_req->get_dest_domain()))
        {
            prepare403block();
            clt_ssl->write(const_cast<char*>(server_http_resp->get_raw_header().c_str()), server_http_resp->get_raw_header().size(), NULL);
            clt_ssl->write(const_cast<char*>(resp_body.c_str()), resp_body.size(), NULL);
            return err;
        }

        //send request header to server
        svr_ssl->write(const_cast<char*>(client_http_req->get_raw_header().c_str()), client_http_req->get_raw_header().size(), NULL);

        //check whether need to forward body
        if(client_http_req->is_chunked() || client_http_req->content_length() > 0)
        {
            client_http_req->body_read_all(req_body);
        }

        srs_trace("client req is chunked: %d, content_length: %d", client_http_req->is_chunked(), client_http_req->content_length());
        if(client_http_req->is_chunked())
        {
            char temp[32];
            snprintf(temp, sizeof(temp), "%x", req_body.size());
            srs_trace("write chunk data to server, chunk size is %s", temp);

            svr_ssl->write(temp, strlen(temp), NULL);
            svr_ssl->write(const_cast<char*>("\r\n"), 2, NULL);
            svr_ssl->write(const_cast<char*>(req_body.c_str()), req_body.size(), NULL);
            svr_ssl->write(const_cast<char*>("\r\n"), 2, NULL);
            svr_ssl->write(const_cast<char*>("0\r\n\r\n"), 5, NULL);
        }
        else if(client_http_req->content_length() > 0)
        {
            srs_trace("write resp_body to client, size is %d", req_body.size());
            svr_ssl->write(const_cast<char*>(req_body.c_str()), req_body.size(), NULL);
        }

        srs_trace("next is to get response from server");
        // get response from server
        // current, we are sure to get http header, body is not sure
        if ((err = server_parser->initialize(HTTP_RESPONSE)) != srs_success) {
            return srs_error_wrap(err, "init parser for %s", ip.c_str());
        }
        srs_trace("parse msg from server");
        ISrsHttpMessage* server_resp = NULL;
        if ((err = server_parser->parse_message(svr_ssl, &server_resp)) != srs_success) {
            return srs_error_wrap(err, "parse message");
        }
        SrsAutoFree(ISrsHttpMessage, server_resp);
        // send response to client
        server_http_resp = (SrsHttpMessage*)server_resp;
        clt_ssl->write(const_cast<char*>(server_http_resp->get_raw_header().c_str()), server_http_resp->get_raw_header().size(), NULL);

        if(server_http_resp->is_chunked() || server_http_resp->content_length() > 0)
        {
            while(1)
            {
                resp_body = "";
                srs_trace("start to read body");
                int finish = 0;
                err = server_http_resp->body_read_part(resp_body, 4096, finish);
                srs_trace("server req is chunked: %d, content_length: %d", server_http_resp->is_chunked(), server_http_resp->content_length());

                if(server_http_resp->is_chunked())
                {
                    char temp[32];
                    snprintf(temp, sizeof(temp), "%x", resp_body.size());
                    srs_trace("write chunk data to client, chunk size is %s", temp);

                    clt_ssl->write(temp, strlen(temp), NULL);
                    clt_ssl->write(const_cast<char*>("\r\n"), 2, NULL);
                    clt_ssl->write(const_cast<char*>(resp_body.c_str()), resp_body.size(), NULL);
                    clt_ssl->write(const_cast<char*>("\r\n"), 2, NULL);
                }
                else if(server_http_resp->content_length() > 0)
                {
                    srs_trace("resp_body is %d", resp_body.size());
                    clt_ssl->write(const_cast<char*>(resp_body.c_str()), resp_body.size(), NULL);
                }
                if(finish)
                {
                    srs_trace("meet eof");
                    clt_ssl->write(const_cast<char*>("0\r\n\r\n"), 5, NULL);
                    break;
                }
            }
        }  

        // if(server_http_resp->is_chunked() || server_http_resp->content_length() > 0)
        // {
        //     server_http_resp->body_read_all(resp_body);
        // }

        // srs_trace("server resp is chunked: %d, content_length: %d", server_http_resp->is_chunked(), server_http_resp->content_length());
        // if(server_http_resp->is_chunked())
        // {
        //     char temp[32];
        //     snprintf(temp, sizeof(temp), "%x", resp_body.size());
        //     srs_trace("write chunk data to client, chunk size is %s", temp);

        //     clt_ssl->write(temp, strlen(temp), NULL);
        //     clt_ssl->write(const_cast<char*>("\r\n"), 2, NULL);
        //     clt_ssl->write(const_cast<char*>(resp_body.c_str()), resp_body.size(), NULL);
        //     clt_ssl->write(const_cast<char*>("\r\n"), 2, NULL);
        //     clt_ssl->write(const_cast<char*>("0\r\n\r\n"), 5, NULL);
        // }
        // else if(server_http_resp->content_length() > 0)
        // {
        //     srs_trace("write resp_body to client, size is %d", resp_body.size());
        //     clt_ssl->write(const_cast<char*>(resp_body.c_str()), resp_body.size(), NULL);
        // }

        // donot keep alive, disconnect it.
        if (!client_http_req->is_keep_alive() || !server_http_resp->is_keep_alive()) {
            srs_trace("not keep-alive connection, close it now");
            break;
        }

        if(server_http_resp->status_code() == 101)
        {
            //web socket tracffic, tunnel it
            srs_trace("web socket traffic, tunnel it");
            processHttpsTunnel();
        }

        SrsAccessLogInfo* log_info = new SrsAccessLogInfo();
        SrsAutoFree(SrsAccessLogInfo, log_info);
        log_info->domain = client_http_req->get_dest_domain();
        log_info->client_ip = remote_ip();
        log_info->status_code = server_http_resp->status_code();
        _srs_access_log->write_access_log(log_info);
        srs_trace("one https transaction done, wait next");

        req_body = "";
        resp_body = "";
        client_http_req = NULL;
        server_http_resp = NULL;
    }

    return err;
}

int SrsHttpxProxyConn::pass(ISrsProtocolReadWriter* in, ISrsProtocolReadWriter* out)
{
    srs_trace("pass");
    srs_error_t err;
    int IOBUFSIZE = 4096;
	char buf[IOBUFSIZE];
	ssize_t read_size = 0;
    ssize_t write_size = 0;

	err = in->read(buf, IOBUFSIZE, &read_size);
    srs_trace("read size is %zd", read_size);
	if (read_size <= 0)
		return -1;

	err = out->write(buf, read_size, &write_size);
    srs_trace("write size is %zd", read_size);
	if (read_size != read_size)
		return -1;

	return 0;
}

srs_error_t SrsHttpxProxyConn::processHttpsTunnel()
{
	srs_trace("process https tunnel");
    srs_error_t err = srs_success;

	struct pollfd pds[2];

	pds[0].fd = clt_skt->get_fd();
	pds[0].events = POLLIN;

	pds[1].fd = svr_skt->get_fd();
	pds[1].events = POLLIN;
	srs_trace("client fd: %d, server fd: %d", pds[0].fd, pds[1].fd);
	for ( ; ; )
	{
		pds[0].revents = 0;
		pds[1].revents = 0;

		if (srs_poll(pds, 2, SRS_UTIME_NO_TIMEOUT) <= 0)
		{
			srs_trace("st_poll error");
			break;
		}

		if (pds[0].revents & POLLIN)
		{
            srs_trace("client has read events");
			if (pass(clt_skt, svr_skt) < 0)
				break;
		}

		if (pds[1].revents & POLLIN)
		{
            srs_trace("server read events");
			if (pass(svr_skt, clt_skt) < 0)
				break;
		}
	}

	return err;
}

srs_error_t SrsHttpxProxyConn::detect_url_category()
{
    SrsHttpClient hc;
    hc.initialize("http", "127.0.0.1", 8081, SRS_UTIME_SECONDS * 5);
    SrsJsonObject* req_obj = SrsJsonAny::object();
    req_obj->set("url", SrsJsonAny::str(client_http_req->uri().c_str()));
    ISrsHttpMessage *resp = NULL;
    hc.post("/url_detect", req_obj->dumps(), &resp);
    std::string req_body_tmp = "";
    
    if(resp != NULL)
    {
        resp->body_read_all(req_body_tmp);

        SrsJsonAny* any = NULL;
        if ((any = SrsJsonAny::loads(req_body_tmp)) == NULL) {
            srs_error("load error");
        }

        SrsJsonObject *obj_req = NULL;
        SrsAutoFree(SrsJsonObject, obj_req);

        if(any->is_object())
        {
            obj_req = any->to_object();
        }

        SrsJsonAny* prop = obj_req->get_property("category");
        std::string category = prop->to_str();

        srs_trace("url category is %s", category.c_str());
    }
}

srs_error_t SrsHttpxProxyConn::on_disconnect()
{
    // TODO: FIXME: Implements it.
    return srs_success;
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
    
    // if ((err = http_static->initialize()) != srs_success) {
    //     return srs_error_wrap(err, "http static");
    // }
    
    return err;
}

srs_error_t SrsHttpServer::handle(std::string pattern, ISrsHttpHandler* handler)
{
    srs_error_t err = srs_success;
    return err;
}

srs_error_t SrsHttpServer::serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r)
{
    srs_error_t err = srs_success;
}

SrsHttpServer::SrsHttpServer(SrsServer* svr)
{
    server = svr;
    // http_static = new SrsHttpStaticServer(svr);
}

SrsHttpServer::~SrsHttpServer()
{
    // srs_freep(http_stream);
    // srs_freep(http_static);
}
