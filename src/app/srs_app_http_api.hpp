#ifndef SRS_APP_HTTP_API_HPP
#define SRS_APP_HTTP_API_HPP

#include <srs_core.hpp>
#include <srs_app_http_api.hpp>
#include <srs_protocol_http_stack.hpp>

class ISrsHttpMessage;
class SrsHttpParser;

// For http root.
class SrsGoApiRoot : public ISrsHttpHandler
{
public:
    SrsGoApiRoot();
    virtual ~SrsGoApiRoot();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiApi : public ISrsHttpHandler
{
public:
    SrsGoApiApi();
    virtual ~SrsGoApiApi();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiVersion : public ISrsHttpHandler
{
public:
    SrsGoApiVersion();
    virtual ~SrsGoApiVersion();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};


class SrsMytest : public ISrsHttpHandler
{
public:
    SrsMytest();
    virtual ~SrsMytest();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};
#endif
