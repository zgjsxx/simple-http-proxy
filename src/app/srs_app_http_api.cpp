//
// Copyright (c) 2013-2022 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//

#include <srs_app_http_api.hpp>
#include <srs_protocol_json.hpp>
#include <srs_kernel_error.hpp>
#include <srs_core_auto_free.hpp>
#include <srs_kernel_log.hpp>

srs_error_t srs_api_response_jsonp(ISrsHttpResponseWriter* w, string callback, string data)
{
    srs_error_t err = srs_success;
    
    SrsHttpHeader* h = w->header();
    
    h->set_content_length(data.length() + callback.length() + 2);
    h->set_content_type("text/javascript");
    
    if (!callback.empty() && (err = w->write((char*)callback.data(), (int)callback.length())) != srs_success) {
        return srs_error_wrap(err, "write jsonp callback");
    }
    
    static char* c0 = (char*)"(";
    if ((err = w->write(c0, 1)) != srs_success) {
        return srs_error_wrap(err, "write jsonp left token");
    }
    if ((err = w->write((char*)data.data(), (int)data.length())) != srs_success) {
        return srs_error_wrap(err, "write jsonp data");
    }
    
    static char* c1 = (char*)")";
    if ((err = w->write(c1, 1)) != srs_success) {
        return srs_error_wrap(err, "write jsonp right token");
    }
    
    return err;
}

srs_error_t srs_api_response_json(ISrsHttpResponseWriter* w, string data)
{
    srs_error_t err = srs_success;
    
    SrsHttpHeader* h = w->header();
    
    h->set_content_length(data.length());
    h->set_content_type("application/json");
    
    if ((err = w->write((char*)data.data(), (int)data.length())) != srs_success) {
        return srs_error_wrap(err, "write json");
    }
    
    return err;
}

srs_error_t srs_api_response(ISrsHttpResponseWriter* w, ISrsHttpMessage* r, std::string json)
{
    // no jsonp, directly response.
    if (!r->is_jsonp()) {
        return srs_api_response_json(w, json);
    }
    
    // jsonp, get function name from query("callback")
    string callback = r->query_get("callback");
    return srs_api_response_jsonp(w, callback, json);
}

SrsGoApiRoot::SrsGoApiRoot()
{
}

SrsGoApiRoot::~SrsGoApiRoot()
{
}

srs_error_t SrsGoApiRoot::serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r)
{
    // SrsStatistic* stat = SrsStatistic::instance();
    
    // SrsJsonObject* obj = SrsJsonAny::object();
    // SrsAutoFree(SrsJsonObject, obj);
    
    // obj->set("code", SrsJsonAny::integer(ERROR_SUCCESS));
    // obj->set("server", SrsJsonAny::str(stat->server_id().c_str()));
    
    // SrsJsonObject* urls = SrsJsonAny::object();
    // obj->set("urls", urls);
    
    // urls->set("api", SrsJsonAny::str("the api root"));

    // if (true) {
    //     SrsJsonObject* rtc = SrsJsonAny::object();
    //     urls->set("rtc", rtc);

    //     SrsJsonObject* v1 = SrsJsonAny::object();
    //     rtc->set("v1", v1);

    //     v1->set("play", SrsJsonAny::str("Play stream"));
    //     v1->set("publish", SrsJsonAny::str("Publish stream"));
    //     v1->set("nack", SrsJsonAny::str("Simulate the NACK"));
    // }

    // return srs_api_response(w, r, obj->dumps());
}

SrsGoApiApi::SrsGoApiApi()
{
}

SrsGoApiApi::~SrsGoApiApi()
{
}

srs_error_t SrsGoApiApi::serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r)
{
    //simple example of read json request
    std::string req_body;
    srs_trace("raw req_body");
    r->body_read_all(req_body);
    srs_trace("raw req_body is %s", req_body.c_str());

    SrsJsonAny* any = NULL;
    if ((any = SrsJsonAny::loads(req_body)) == NULL) {
        srs_trace("!=NULL");
        return srs_success;
    }
    srs_trace("129");
    SrsJsonObject *obj_req = NULL;
    SrsAutoFree(SrsJsonObject, obj_req);
    if(any->is_object())
    {
        obj_req = any->to_object();
    }
    srs_trace("135");
    //Jmeter test failed when add this two line
    if(obj_req->get_property("msg") != NULL)
    {
        std::string msg = obj_req->get_property("msg")->to_str();
        srs_trace("msg = %s", msg.c_str());
    }
    
    srs_trace("140");

    SrsJsonObject* obj = SrsJsonAny::object();
    SrsAutoFree(SrsJsonObject, obj);
    
    obj->set("code", SrsJsonAny::integer(ERROR_SUCCESS));
    SrsJsonObject* urls = SrsJsonAny::object();
    obj->set("urls", urls);
    urls->set("v1", SrsJsonAny::str("the api version 1.0"));
    
    return srs_api_response(w, r, obj->dumps());
}

SrsGoApiVersion::SrsGoApiVersion()
{
}

SrsGoApiVersion::~SrsGoApiVersion()
{
}

srs_error_t SrsGoApiVersion::serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r)
{
    // SrsStatistic* stat = SrsStatistic::instance();
    
    SrsJsonObject* obj = SrsJsonAny::object();
    SrsAutoFree(SrsJsonObject, obj);
    
    obj->set("code", SrsJsonAny::integer(ERROR_SUCCESS));
    // obj->set("server", SrsJsonAny::str(stat->server_id().c_str()));
    
    SrsJsonObject* data = SrsJsonAny::object();
    obj->set("data", data);
    
    data->set("major", SrsJsonAny::integer(VERSION_MAJOR));
    data->set("minor", SrsJsonAny::integer(VERSION_MINOR));
    data->set("revision", SrsJsonAny::integer(VERSION_REVISION));
    data->set("version", SrsJsonAny::str(RTMP_SIG_SRS_VERSION));
    
    return srs_api_response(w, r, obj->dumps());
}

SrsMytest::SrsMytest()
{
}

SrsMytest::~SrsMytest()
{
}

srs_error_t SrsMytest::serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r)
{
    // SrsStatistic* stat = SrsStatistic::instance();
    
    SrsJsonObject* obj = SrsJsonAny::object();
    SrsAutoFree(SrsJsonObject, obj);
    
    obj->set("code", SrsJsonAny::integer(ERROR_SUCCESS));
    obj->set("msg", SrsJsonAny::str("Hello"));
    
    return srs_api_response(w, r, obj->dumps());
}