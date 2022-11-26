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
#include <srs_app_utility.hpp>

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

SrsGoApiSystemProcStats::SrsGoApiSystemProcStats()
{
}

SrsGoApiSystemProcStats::~SrsGoApiSystemProcStats()
{
}

srs_error_t SrsGoApiSystemProcStats::serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r)
{   
    // SrsJsonObject* obj = SrsJsonAny::object();
    // SrsAutoFree(SrsJsonObject, obj);
    
    // obj->set("code", SrsJsonAny::integer(ERROR_SUCCESS));
    
    // SrsJsonObject* data = SrsJsonAny::object();
    // obj->set("data", data);
    
    // SrsProcSystemStat* s = srs_get_system_proc_stat();
    
    // data->set("ok", SrsJsonAny::boolean(s->ok));
    // data->set("sample_time", SrsJsonAny::integer(s->sample_time));
    // data->set("percent", SrsJsonAny::number(s->percent));
    // data->set("user", SrsJsonAny::integer(s->user));
    // data->set("nice", SrsJsonAny::integer(s->nice));
    // data->set("sys", SrsJsonAny::integer(s->sys));
    // data->set("idle", SrsJsonAny::integer(s->idle));
    // data->set("iowait", SrsJsonAny::integer(s->iowait));
    // data->set("irq", SrsJsonAny::integer(s->irq));
    // data->set("softirq", SrsJsonAny::integer(s->softirq));
    // data->set("steal", SrsJsonAny::integer(s->steal));
    // data->set("guest", SrsJsonAny::integer(s->guest));
    
    // return srs_api_response(w, r, obj->dumps());
}

SrsGoApiSelfProcStats::SrsGoApiSelfProcStats()
{
}

SrsGoApiSelfProcStats::~SrsGoApiSelfProcStats()
{
}

srs_error_t SrsGoApiSelfProcStats::serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r)
{
    // SrsStatistic* stat = SrsStatistic::instance();
    
    SrsJsonObject* obj = SrsJsonAny::object();
    SrsAutoFree(SrsJsonObject, obj);
    
    obj->set("code", SrsJsonAny::integer(ERROR_SUCCESS));
    // obj->set("server", SrsJsonAny::str(stat->server_id().c_str()));
    
    SrsJsonObject* data = SrsJsonAny::object();
    obj->set("data", data);
    
    SrsProcSelfStat* u = srs_get_self_proc_stat();
    
    string state;
    state += (char)u->state;
    
    data->set("ok", SrsJsonAny::boolean(u->ok));
    data->set("sample_time", SrsJsonAny::integer(u->sample_time));
    data->set("percent", SrsJsonAny::number(u->percent));
    data->set("pid", SrsJsonAny::integer(u->pid));
    data->set("comm", SrsJsonAny::str(u->comm));
    data->set("state", SrsJsonAny::str(state.c_str()));
    data->set("ppid", SrsJsonAny::integer(u->ppid));
    data->set("pgrp", SrsJsonAny::integer(u->pgrp));
    data->set("session", SrsJsonAny::integer(u->session));
    data->set("tty_nr", SrsJsonAny::integer(u->tty_nr));
    data->set("tpgid", SrsJsonAny::integer(u->tpgid));
    data->set("flags", SrsJsonAny::integer(u->flags));
    data->set("minflt", SrsJsonAny::integer(u->minflt));
    data->set("cminflt", SrsJsonAny::integer(u->cminflt));
    data->set("majflt", SrsJsonAny::integer(u->majflt));
    data->set("cmajflt", SrsJsonAny::integer(u->cmajflt));
    data->set("utime", SrsJsonAny::integer(u->utime));
    data->set("stime", SrsJsonAny::integer(u->stime));
    data->set("cutime", SrsJsonAny::integer(u->cutime));
    data->set("cstime", SrsJsonAny::integer(u->cstime));
    data->set("priority", SrsJsonAny::integer(u->priority));
    data->set("nice", SrsJsonAny::integer(u->nice));
    data->set("num_threads", SrsJsonAny::integer(u->num_threads));
    data->set("itrealvalue", SrsJsonAny::integer(u->itrealvalue));
    data->set("starttime", SrsJsonAny::integer(u->starttime));
    data->set("vsize", SrsJsonAny::integer(u->vsize));
    data->set("rss", SrsJsonAny::integer(u->rss));
    data->set("rsslim", SrsJsonAny::integer(u->rsslim));
    data->set("startcode", SrsJsonAny::integer(u->startcode));
    data->set("endcode", SrsJsonAny::integer(u->endcode));
    data->set("startstack", SrsJsonAny::integer(u->startstack));
    data->set("kstkesp", SrsJsonAny::integer(u->kstkesp));
    data->set("kstkeip", SrsJsonAny::integer(u->kstkeip));
    data->set("signal", SrsJsonAny::integer(u->signal));
    data->set("blocked", SrsJsonAny::integer(u->blocked));
    data->set("sigignore", SrsJsonAny::integer(u->sigignore));
    data->set("sigcatch", SrsJsonAny::integer(u->sigcatch));
    data->set("wchan", SrsJsonAny::integer(u->wchan));
    data->set("nswap", SrsJsonAny::integer(u->nswap));
    data->set("cnswap", SrsJsonAny::integer(u->cnswap));
    data->set("exit_signal", SrsJsonAny::integer(u->exit_signal));
    data->set("processor", SrsJsonAny::integer(u->processor));
    data->set("rt_priority", SrsJsonAny::integer(u->rt_priority));
    data->set("policy", SrsJsonAny::integer(u->policy));
    data->set("delayacct_blkio_ticks", SrsJsonAny::integer(u->delayacct_blkio_ticks));
    data->set("guest_time", SrsJsonAny::integer(u->guest_time));
    data->set("cguest_time", SrsJsonAny::integer(u->cguest_time));
    
    return srs_api_response(w, r, obj->dumps());
}
