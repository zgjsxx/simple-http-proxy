#include <string.h>
#include <vector>
#include <srs_protocol_http_conn.hpp>
#include <srs_protocol_utility.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_core_platform.hpp>
#include <srs_kernel_log.hpp>
#include <srs_kernel_consts.hpp>

using std::vector;

SrsHttpParser::SrsHttpParser()
{
    buffer = new SrsFastStream();
    header = NULL;

    p_body_start = p_header_tail = NULL;
    type_ = HTTP_REQUEST;
}

SrsHttpParser::~SrsHttpParser()
{
    srs_freep(buffer);
    srs_freep(header);
}

srs_error_t SrsHttpParser::initialize(enum http_parser_type type)
{
    srs_error_t err = srs_success;

    jsonp = false;
    type_ = type;

    // Initialize the parser, however it's not necessary.
    http_parser_init(&parser, type_);
    parser.data = (void*)this;
    
    memset(&settings, 0, sizeof(settings));
    settings.on_message_begin = on_message_begin;
    settings.on_url = on_url;
    settings.on_header_field = on_header_field;
    settings.on_header_value = on_header_value;
    settings.on_headers_complete = on_headers_complete;
    settings.on_body = on_body;
    settings.on_message_complete = on_message_complete;
    
    return err;
}

void SrsHttpParser::set_jsonp(bool allow_jsonp)
{
    jsonp = allow_jsonp;
}

srs_error_t SrsHttpParser::parse_message(ISrsReader* reader, ISrsHttpMessage** ppmsg)
{
    srs_error_t err = srs_success;

    *ppmsg = NULL;
    
    // Reset request data.
    state = SrsHttpParseStateInit;
    memset(&hp_header, 0, sizeof(http_parser));
    // The body that we have read from cache.
    p_body_start = p_header_tail = NULL;
    // We must reset the field name and value, because we may get a partial value in on_header_value.
    field_name = field_value = "";
    // Reset the url.
    url = "";
    // The header of the request.
    srs_freep(header);
    header = new SrsHttpHeader();

    // Reset parser for each message.
    // If the request is large, such as the fifth message at @utest ProtocolHTTPTest.ParsingLargeMessages,
    // we got header and part of body, so the parser will stay at SrsHttpParseStateBody:
    //      ***MESSAGE BEGIN***
    //      ***HEADERS COMPLETE***
    //      Body: xxx
    // when got next message, the whole next message is parsed as the body of previous one,
    // and the message fail.
    // @note You can comment the bellow line, the utest will fail.
    http_parser_init(&parser, type_);
    // callback object ptr.
    parser.data = (void*)this;
    
    // do parse
    if ((err = parse_message_imp(reader)) != srs_success) {
        return srs_error_wrap(err, "parse message");
    }
    
    // create msg
    SrsHttpMessage* msg = new SrsHttpMessage(reader, buffer);

    // Initialize the basic information.

    msg->set_basic(hp_header.type, hp_header.method, hp_header.status_code, hp_header.content_length);
    msg->set_header(header, http_should_keep_alive(&hp_header));
    // For HTTP response, no url.
    if (type_ != HTTP_RESPONSE && (err = msg->set_url(url, jsonp)) != srs_success) {
        srs_freep(msg);
        return srs_error_wrap(err, "set url=%s, jsonp=%d", url.c_str(), jsonp);
    }
    //get host value and port from the header
    if(type_ == HTTP_REQUEST) {
        msg->get_host_port();
    }
    //raw http header used to forward to server
    msg->restore_http_header();

    // parse ok, return the msg.
    *ppmsg = msg;
    
    return err;
}

srs_error_t SrsHttpParser::parse_message_imp(ISrsReader* reader)
{
    srs_error_t err = srs_success;
    
    while (true) {
        if (buffer->size() > 0) {
            ssize_t consumed = http_parser_execute(&parser, &settings, buffer->bytes(), buffer->size());
            // The error is set in http_errno.
            enum http_errno code;
	        if ((code = HTTP_PARSER_ERRNO(&parser)) != HPE_OK) {
	            return srs_error_new(ERROR_HTTP_PARSE_HEADER, "parse %dB, nparsed=%d, err=%d/%s %s",
	                buffer->size(), (int)consumed, code, http_errno_name(code), http_errno_description(code));
	        }

            // When buffer consumed these bytes, it's dropped so the new ptr is actually the HTTP body. But http-parser
            // doesn't indicate the specific sizeof header, so we must finger it out.
            // @remark We shouldn't use on_body, because it only works for normal case, and losts the chunk header and length.
            // @see https://github.com/ossrs/srs/issues/1508
	        if (p_header_tail && buffer->bytes() < p_body_start) {
	            for (const char* p = p_header_tail; p <= p_body_start - 4; p++) {
	                if (p[0] == SRS_CONSTS_CR && p[1] == SRS_CONSTS_LF && p[2] == SRS_CONSTS_CR && p[3] == SRS_CONSTS_LF) {
	                    consumed = p + 4 - buffer->bytes();
	                    break;
	                }
	            }
	        }
            
            srs_trace("size=%d, nparsed=%d", buffer->size(), (int)consumed);

	        // Only consume the header bytes.
            buffer->read_slice(consumed);

	        // Done when header completed, never wait for body completed, because it maybe chunked.
	        if (state >= SrsHttpParseStateHeaderComplete) {
	            break;
	        }
        }
        
        // when nothing parsed, read more to parse.
        // when requires more, only grow 1bytes, but the buffer will cache more.
        if ((err = buffer->grow(reader, buffer->size() + 1)) != srs_success) {
            return srs_error_wrap(err, "grow buffer");
        }
    }

    SrsHttpParser* obj = this;
    if (!obj->field_value.empty()) {
        obj->header->set(obj->field_name, obj->field_value);
    }
    
    return err;
}

int SrsHttpParser::on_message_begin(http_parser* parser)
{
    SrsHttpParser* obj = (SrsHttpParser*)parser->data;
    srs_assert(obj);
    
    obj->state = SrsHttpParseStateStart;
    
    srs_info("***MESSAGE BEGIN***");
    
    return 0;
}

int SrsHttpParser::on_headers_complete(http_parser* parser)
{
    SrsHttpParser* obj = (SrsHttpParser*)parser->data;
    srs_assert(obj);
    
    obj->hp_header = *parser;
    // save the parser when header parse completed.
    obj->state = SrsHttpParseStateHeaderComplete;

    // We must update the body start when header complete, because sometimes we only got header.
    // When we got the body start event, we will update it to much precious position.
    obj->p_body_start = obj->buffer->bytes() + obj->buffer->size();

    srs_trace("***HEADERS COMPLETE***");
    
    // see http_parser.c:1570, return 1 to skip body.
    return 0;
}

int SrsHttpParser::on_message_complete(http_parser* parser)
{
    SrsHttpParser* obj = (SrsHttpParser*)parser->data;
    srs_assert(obj);
    
    // save the parser when body parse completed.
    obj->state = SrsHttpParseStateMessageComplete;
    
    srs_info("***MESSAGE COMPLETE***\n");
    
    return 0;
}

int SrsHttpParser::on_url(http_parser* parser, const char* at, size_t length)
{
    SrsHttpParser* obj = (SrsHttpParser*)parser->data;
    srs_assert(obj);

    if (length > 0) {
        // Note that this function might be called for multiple times, and we got pieces of content.
        obj->url.append(at, (int)length);
    }

    // When header parsed, we must save the position of start for body,
    // because we have to consume the header in buffer.
    // @see https://github.com/ossrs/srs/issues/1508
    obj->p_header_tail = at;
    
    srs_trace("Method: %d, Url: %.*s", parser->method, (int)length, at);
    
    return 0;
}

int SrsHttpParser::on_header_field(http_parser* parser, const char* at, size_t length)
{
    SrsHttpParser* obj = (SrsHttpParser*)parser->data;
    srs_assert(obj);

    //response header can have multiple Set-Cookie headers
    if (!obj->field_name.empty()) {
        if(obj->field_name == "Set-Cookie" || obj->field_name == "set-cookie")
        {
            obj->header->addCookie(obj->field_value);
        }
        else
        {
            obj->header->set(obj->field_name, obj->field_value);
        }

        obj->field_name = obj->field_value = "";
    }
    
    if (length > 0) {
        obj->field_name.append(at, (int)length);
    }

    // When header parsed, we must save the position of start for body,
    // because we have to consume the header in buffer.
    // @see https://github.com/ossrs/srs/issues/1508
    obj->p_header_tail = at;
    
    srs_info("Header field(%d bytes): %.*s", (int)length, (int)length, at);
    return 0;
}

int SrsHttpParser::on_header_value(http_parser* parser, const char* at, size_t length)
{
    SrsHttpParser* obj = (SrsHttpParser*)parser->data;
    srs_assert(obj);
    
    if (length > 0) {
        obj->field_value.append(at, (int)length);

        // When header parsed, we must save the position of start for body,
        // because we have to consume the header in buffer.
        // @see https://github.com/ossrs/srs/issues/1508
        obj->p_header_tail = at;
    }
    
    srs_info("Header value(%d bytes): %.*s", (int)length, (int)length, at);
    return 0;
}

int SrsHttpParser::on_body(http_parser* parser, const char* at, size_t length)
{
    SrsHttpParser* obj = (SrsHttpParser*)parser->data;
    srs_assert(obj);

    // save the parser when body parsed.
    obj->state = SrsHttpParseStateBody;

    // Used to discover the header length.
    // @see https://github.com/ossrs/srs/issues/1508
    obj->p_body_start = at;

    srs_info("Body: %.*s", (int)length, at);
    
    return 0;
}

SrsHttpResponseReader::SrsHttpResponseReader(SrsHttpMessage* msg, ISrsReader* reader, SrsFastStream* body)
{
    skt = reader;
    owner = msg;
    is_eof = false;
    nb_total_read = 0;
    nb_left_chunk = 0;
    buffer = body;
    nb_chunk = 0;    
}

SrsHttpResponseReader::~SrsHttpResponseReader()
{
}

void SrsHttpResponseReader::close()
{
    is_eof = true;
}

bool SrsHttpResponseReader::eof()
{
    return is_eof;
}

srs_error_t SrsHttpResponseReader::read(void* data, size_t nb_data, ssize_t* nb_read)
{
    srs_error_t err = srs_success;
    if (is_eof) {
        return srs_error_new(ERROR_HTTP_RESPONSE_EOF, "EOF");
    }

    // chunked encoding.
    if (owner->is_chunked()) {
        return read_chunked(data, nb_data, nb_read);
    }

    // read by specified content-length
    if (owner->content_length() != -1) {
        size_t max = (size_t)owner->content_length() - (size_t)nb_total_read;
        if (max <= 0) {
            is_eof = true;
            return err;
        }
        
        // change the max to read.
        nb_data = srs_min(nb_data, max);
        return read_specified(data, nb_data, nb_read);
    }
    
    // Infinite chunked mode.
    // If not chunked encoding, and no content-length, it's infinite chunked.
    // In this mode, all body is data and never EOF util socket closed.
    if ((err = read_specified(data, nb_data, nb_read)) != srs_success) {
        // For infinite chunked, the socket close event is EOF.
        if (srs_error_code(err) == ERROR_SOCKET_READ) {
            srs_freep(err); is_eof = true;
            return err;
        }
    }

    return err;
}

srs_error_t SrsHttpResponseReader::read_chunked(void* data, size_t nb_data, ssize_t* nb_read)
{
    srs_error_t err = srs_success;
    
    // when no bytes left in chunk,
    // parse the chunk length first.
    if (nb_left_chunk <= 0) {
        char* at = NULL;
        int length = 0;
        while (!at) {
            // find the CRLF of chunk header end.
            char* start = buffer->bytes();
            char* end = start + buffer->size();
            for (char* p = start; p < end - 1; p++) {
                if (p[0] == SRS_HTTP_CR && p[1] == SRS_HTTP_LF) {
                    // invalid chunk, ignore.
                    if (p == start) {
                        return srs_error_new(ERROR_HTTP_INVALID_CHUNK_HEADER, "chunk header");
                    }
                    length = (int)(p - start + 2);
                    at = buffer->read_slice(length);
                    break;
                }
            }
            
            // got at, ok.
            if (at) {
                break;
            }
            
            // when empty, only grow 1bytes, but the buffer will cache more.
            if ((err = buffer->grow(skt, buffer->size() + 1)) != srs_success) {
                return srs_error_wrap(err, "grow buffer");
            }
        }
        srs_assert(length >= 3);
        
        // it's ok to set the pos and pos+1 to NULL.
        at[length - 1] = 0;
        at[length - 2] = 0;

        // size is the bytes size, excludes the chunk header and end CRLF.
        // @remark It must be hex format, please read https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Transfer-Encoding#Directives
        // @remark For strtol, note that: If no conversion could be performed, 0 is returned and the global variable errno is set to EINVAL.
        char* at_parsed = at; errno = 0;
        int ilength = (int)::strtol(at, &at_parsed, 16);
        if (ilength < 0 || errno != 0 || at_parsed - at != length - 2) {
            return srs_error_new(ERROR_HTTP_INVALID_CHUNK_HEADER, "invalid length %s as %d, parsed=%.*s, errno=%d",
                at, ilength, (int)(at_parsed-at), at, errno);
        }
        
        // all bytes in chunk is left now.
        nb_chunk = nb_left_chunk = (size_t)ilength;
    }
    
    if (nb_chunk <= 0) {
        // for the last chunk, eof.
        is_eof = true;
        *nb_read = 0;
    } else {
        // for not the last chunk, there must always exists bytes.
        // left bytes in chunk, read some.
        srs_assert(nb_left_chunk);
        
        size_t nb_bytes = srs_min(nb_left_chunk, nb_data);
        err = read_specified(data, nb_bytes, (ssize_t*)&nb_bytes);
        
        // the nb_bytes used for output already read size of bytes.
        if (nb_read) {
            *nb_read = nb_bytes;
        }
        nb_left_chunk -= nb_bytes;

        if (err != srs_success) {
            return srs_error_wrap(err, "read specified");
        }

        // If still left bytes in chunk, ignore and read in future.
        if (nb_left_chunk > 0) {
            return err;
        }
    }
    
    // for both the last or not, the CRLF of chunk payload end.
    if ((err = buffer->grow(skt, 2)) != srs_success) {
        return srs_error_wrap(err, "grow buffer");
    }
    buffer->read_slice(2);
    
    return err;
}

srs_error_t SrsHttpResponseReader::read_specified(void* data, size_t nb_data, ssize_t* nb_read)
{
    srs_error_t err = srs_success;
    if (buffer->size() <= 0) {
        // when empty, only grow 1bytes, but the buffer will cache more.
        if ((err = buffer->grow(skt, 1)) != srs_success) {
            return srs_error_wrap(err, "grow buffer");
        }
    }
    size_t nb_bytes = srs_min(nb_data, (size_t)buffer->size());
    // read data to buffer.
    srs_assert(nb_bytes);

    char* p = buffer->read_slice(nb_bytes);
    memcpy(data, p, nb_bytes);
    if (nb_read) {
        *nb_read = nb_bytes;
    }

    // increase the total read to determine whether EOF.
    nb_total_read += nb_bytes;
    // for not chunked and specified content length.
    if (!owner->is_chunked() && owner->content_length() != -1) {
        // when read completed, eof.
        if (nb_total_read >= (int)owner->content_length()) {
            is_eof = true;
        }
    }
    
    return err;
}

SrsHttpMessage::SrsHttpMessage(ISrsReader* reader, SrsFastStream* buffer) : ISrsHttpMessage()
{
    // owner_conn = NULL;
    chunked = false;
    _uri = new SrsHttpUri();
    _body = new SrsHttpResponseReader(this, reader, buffer);

    jsonp = false;

    // As 0 is DELETE, so we use GET as default.
    _method = SRS_CONSTS_HTTP_GET;
    // 200 is ok.
    _status = SRS_CONSTS_HTTP_OK;
    // -1 means infinity chunked mode.
    _content_length = -1;
    // From HTTP/1.1, default to keep alive.
    _keep_alive = true;
    type_ = 0;

    schema_ = "http";
}

SrsHttpMessage::~SrsHttpMessage()
{
    srs_freep(_body);
    srs_freep(_uri);
}

void SrsHttpMessage::set_basic(uint8_t type, uint8_t method, uint16_t status, int64_t content_length)
{
    type_ = type;
    _method = method;
    _status = status;
    if (_content_length == -1) {
        _content_length = content_length;
    }
}

void SrsHttpMessage::set_header(SrsHttpHeader* header, bool keep_alive)
{
    _header = *header;
    _keep_alive = keep_alive;

    // whether chunked.
    chunked = (header->get("Transfer-Encoding") == "chunked");

    // Update the content-length in header.
    string clv = header->get("Content-Length");
    if (!clv.empty()) {
        _content_length = ::atoll(clv.c_str());
    }

    // If no size(content-length or chunked), it's infinite chunked,
    // it means there is no body, so we must close the body reader.
    if (!chunked && _content_length == -1) {
        // The infinite chunked is only enabled for HTTP_RESPONSE, so we close the body for request.
        if (type_ == HTTP_REQUEST) {
            _body->close();
        }
    }
}

string SrsHttpMessage::query_get(string key)
{
    std::string v;
    
    if (_query.find(key) != _query.end()) {
        v = _query[key];
    }
    
    return v;
}

srs_error_t SrsHttpMessage::set_url(string url, bool allow_jsonp)
{
    srs_error_t err = srs_success;
    
    _url = url;

    if(is_http_connect())
    {
        return err;
    }

    // parse uri from schema/server:port/path?query
    std::string uri = _url;

    if (!srs_string_contains(uri, "://")) {
        // use server public ip when host not specified.
        // to make telnet happy.
        std::string host = _header.get("Host");

        // If no host in header, we use local discovered IP, IPv4 first.
        if (host.empty()) {
            host = srs_get_public_internet_address(true);
        }

        if (!host.empty()) {
            uri = "http://" + host + _url;
        }
    }

    if ((err = _uri->initialize(uri)) != srs_success) {
        return srs_error_wrap(err, "init uri %s", uri.c_str());
    }
    
    // parse ext.
    _ext = srs_path_filext(_uri->get_path());
    
    // parse query string.
    srs_parse_query_string(_uri->get_query(), _query);
    
    // parse jsonp request message.
    if (allow_jsonp) {
        if (!query_get("callback").empty()) {
            jsonp = true;
        }
        if (jsonp) {
            jsonp_method = query_get("method");
        }
    }
    
    return err;
}

void SrsHttpMessage::set_https(bool v)
{
    schema_ = v? "https" : "http";
    _uri->set_schema(schema_);
}

bool SrsHttpMessage::is_chunked()
{
    return chunked;
}

int64_t SrsHttpMessage::content_length()
{
    return _content_length;
}

void SrsHttpMessage::set_connection(ISrsConnection* conn)
{
    owner_conn = conn;
}

string SrsHttpMessage::url()
{
    return _uri->get_url();
}

string SrsHttpMessage::host()
{
    std::map<string, string>::iterator it = _query.find("vhost");
    if (it != _query.end() && !it->second.empty()) {
        return it->second;
    }

    it = _query.find("domain");
    if (it != _query.end() && !it->second.empty()) {
        return it->second;
    }

    return _uri->get_host();
}

string SrsHttpMessage::path()
{
    return _uri->get_path();
}

string SrsHttpMessage::query()
{
    return _uri->get_query();
}

bool SrsHttpMessage::is_jsonp()
{
    return jsonp;
}

string SrsHttpMessage::schema()
{
    return schema_;
}

uint8_t SrsHttpMessage::method()
{
    if (jsonp && !jsonp_method.empty()) {
        if (jsonp_method == "GET") {
            return SRS_CONSTS_HTTP_GET;
        } else if (jsonp_method == "PUT") {
            return SRS_CONSTS_HTTP_PUT;
        } else if (jsonp_method == "POST") {
            return SRS_CONSTS_HTTP_POST;
        } else if (jsonp_method == "DELETE") {
            return SRS_CONSTS_HTTP_DELETE;
        }
    }

    return _method;
}

uint16_t SrsHttpMessage::status_code()
{
    return _status;
}

string SrsHttpMessage::method_str()
{
    if (jsonp && !jsonp_method.empty()) {
        return jsonp_method;
    }
    
    if (is_http_get()) {
        return "GET";
    }
    if (is_http_put()) {
        return "PUT";
    }
    if (is_http_post()) {
        return "POST";
    }
    if (is_http_delete()) {
        return "DELETE";
    }
    if (is_http_options()) {
        return "OPTIONS";
    }
    
    return "OTHER";
}

srs_error_t SrsHttpMessage::body_read_all(string& body)
{
    //process Transfer-Encoding: Trunked
    return srs_ioutil_read_all(_body, body);
}

srs_error_t SrsHttpMessage::body_read_part(string& body, int read_size, int& finish)
{
    //process Transfer-Encoding: Trunked
    return srs_ioutil_read_part(_body, body, read_size, finish);
}

ISrsHttpResponseReader* SrsHttpMessage::body_reader()
{
    return _body;
}

bool SrsHttpMessage::is_http_get()
{
    return method() == SRS_CONSTS_HTTP_GET;
}

bool SrsHttpMessage::is_http_put()
{
    return method() == SRS_CONSTS_HTTP_PUT;
}

bool SrsHttpMessage::is_http_post()
{
    return method() == SRS_CONSTS_HTTP_POST;
}

bool SrsHttpMessage::is_http_delete()
{
    return method() == SRS_CONSTS_HTTP_DELETE;
}

bool SrsHttpMessage::is_http_options()
{
    return _method == SRS_CONSTS_HTTP_OPTIONS;
}

bool SrsHttpMessage::is_http_connect()
{
    return _method == SRS_CONSTS_HTTP_CONNECT;
}

bool SrsHttpMessage::is_keep_alive()
{
    return _keep_alive;
}

string SrsHttpMessage::uri()
{
    std::string uri = _uri->get_schema();
    if (uri.empty()) {
        uri += "http";
    }
    uri += "://";
    
    uri += host();
    uri += path();
    
    return uri;
}

void SrsHttpMessage::restore_http_header()
{
    stringstream ss;
    if(type_ == HTTP_REQUEST)
    {
        //TODO support more method
        if(_method == SRS_CONSTS_HTTP_GET)
        {
            ss << "GET";
        }
        else if(_method == SRS_CONSTS_HTTP_POST)
        {
            ss << "POST";
        }
        else if(_method == SRS_CONSTS_HTTP_PUT)
        {
            ss << "PUT";
        }
        else if(_method == SRS_CONSTS_HTTP_DELETE)
        {
            ss << "DELETE";
        }
        else if(_method == SRS_CONSTS_HTTP_OPTIONS)
        {
            ss << "OPTIONS";
        }
        else if(_method == SRS_CONSTS_HTTP_CONNECT)
        {
            ss << "CONNECT";
        }

        ss << " ";


        ss << _url;

        ss << " ";

        ss << "HTTP/1.1" << SRS_HTTP_CRLF;
    }

    if(type_ == HTTP_RESPONSE)
    {
        ss << "HTTP/1.1 ";
        ss << _status << " ";
        ss << srs_generate_http_status_text(_status) << SRS_HTTP_CRLF;
    }

    header()->write(ss);

    ss << SRS_HTTP_CRLF;
    
    raw_header = ss.str();

    srs_trace("Http Header: %s", raw_header.c_str());
}

void SrsHttpMessage::get_host_port()
{
    SrsHttpHeader* header = this->header();
    string host_tmp = "";
    vector<string> res;
    srs_trace("is https connect = %d", is_http_connect());
    if(!is_http_connect())
    {
        if((host_tmp = header->get("Host")) != "")
        {
            res = srs_string_split(host_tmp, ":");
            if(res.size() == 1)
            {
                //Host: example.com
                host_no_port = res[0];
                dest_port = 80;
            }
            else
            {
                //Host: example.com:8080
                host_no_port = res[0];
                dest_port = atoi(res[1].c_str());
            }
        }
    }
    else
    {
        //CONNECT example.com:443 HTTP/1.1
        srs_trace("url is %s", _url.c_str());
        res = srs_string_split(_url, ":");
        if(res.size() == 1)
        {
            //Host: example.com
            host_no_port = res[0];
            dest_port = 443;
        }
        else
        {
            //Host: example.com:8080
            host_no_port = res[0];
            dest_port = atoi(res[1].c_str());
        }
    }

}

string SrsHttpMessage::get_dest_domain()
{
    return host_no_port;
}

int SrsHttpMessage::get_dest_port()
{
    return dest_port;
}


string SrsHttpMessage::get_raw_header()
{
    return raw_header;
}

SrsHttpHeader* SrsHttpMessage::header()
{
    return &_header;
}

SrsHttpResponseWriter::SrsHttpResponseWriter(ISrsProtocolReadWriter* io)
{
    skt = io;
    hdr = new SrsHttpHeader();
    header_wrote = false;
    status = SRS_CONSTS_HTTP_OK;
    content_length = -1;
    written = 0;
    header_sent = false;
    nb_iovss_cache = 0;
    iovss_cache = NULL;
    hf = NULL;
}

SrsHttpResponseWriter::~SrsHttpResponseWriter()
{
    srs_freep(hdr);
    srs_freepa(iovss_cache);
}

srs_error_t SrsHttpResponseWriter::final_request()
{
    srs_error_t err = srs_success;

    // write the header data in memory.
    if (!header_wrote) {
        write_header(SRS_CONSTS_HTTP_OK);
    }

    // whatever header is wrote, we should try to send header.
    if ((err = send_header(NULL, 0)) != srs_success) {
        return srs_error_wrap(err, "send header");
    }
    
    // complete the chunked encoding.
    if (content_length == -1) {
        std::stringstream ss;
        ss << 0 << SRS_HTTP_CRLF << SRS_HTTP_CRLF;
        std::string ch = ss.str();
        return skt->write((void*)ch.data(), (int)ch.length(), NULL);
    }
    
    // flush when send with content length
    return write(NULL, 0);
}

srs_error_t SrsHttpResponseWriter::write(char* data, int size)
{
    srs_error_t err = srs_success;
    
    // write the header data in memory.
    if (!header_wrote) {
        if (hdr->content_type().empty()) {
            hdr->set_content_type("text/plain; charset=utf-8");
        }
        if (hdr->content_length() == -1) {
            hdr->set_content_length(size);
        }
        write_header(SRS_CONSTS_HTTP_OK);
    }    
   // whatever header is wrote, we should try to send header.
    if ((err = send_header(data, size)) != srs_success) {
        return srs_error_wrap(err, "send header");
    }
    
    // check the bytes send and content length.
    written += size;
    if (content_length != -1 && written > content_length) {
        return srs_error_new(ERROR_HTTP_CONTENT_LENGTH, "overflow writen=%" PRId64 ", max=%" PRId64, written, content_length);
    }
    
    // ignore NULL content.
    if (!data || size <= 0) {
        return err;
    }
    
    // directly send with content length
    if (content_length != -1) {
        return skt->write((void*)data, size, NULL);
    }
    
    // send in chunked encoding.
    int nb_size = snprintf(header_cache, SRS_HTTP_HEADER_CACHE_SIZE, "%x", size);
    if (nb_size <= 0 || nb_size >= SRS_HTTP_HEADER_CACHE_SIZE) {
        return srs_error_new(ERROR_HTTP_CONTENT_LENGTH, "overflow size=%d, expect=%d", size, nb_size);
    }
    
    iovec iovs[4];
    iovs[0].iov_base = (char*)header_cache;
    iovs[0].iov_len = (int)nb_size;
    iovs[1].iov_base = (char*)SRS_HTTP_CRLF;
    iovs[1].iov_len = 2;
    iovs[2].iov_base = (char*)data;
    iovs[2].iov_len = size;
    iovs[3].iov_base = (char*)SRS_HTTP_CRLF;
    iovs[3].iov_len = 2;
    
    ssize_t nwrite = 0;
    if ((err = skt->writev(iovs, 4, &nwrite)) != srs_success) {
        return srs_error_wrap(err, "write chunk");
    }
    
    return err;
}

srs_error_t SrsHttpResponseWriter::writev(const iovec* iov, int iovcnt, ssize_t* pnwrite)
{
    srs_error_t err = srs_success;
    
    // when header not ready, or not chunked, send one by one.
    if (!header_wrote || content_length != -1) {
        ssize_t nwrite = 0;
        for (int i = 0; i < iovcnt; i++) {
            nwrite += iov[i].iov_len;
            if ((err = write((char*)iov[i].iov_base, (int)iov[i].iov_len)) != srs_success) {
                return srs_error_wrap(err, "writev");
            }
        }
        
        if (pnwrite) {
            *pnwrite = nwrite;
        }
        
        return err;
    }
    // ignore NULL content.
    if (iovcnt <= 0) {
        return err;
    }

    // whatever header is wrote, we should try to send header.
    if ((err = send_header(NULL, 0)) != srs_success) {
        return srs_error_wrap(err, "send header");
    }   
   // send in chunked encoding.
    int nb_iovss = 3 + iovcnt;
    iovec* iovss = iovss_cache;
    if (nb_iovss_cache < nb_iovss) {
        srs_freepa(iovss_cache);
        nb_iovss_cache = nb_iovss;
        iovss = iovss_cache = new iovec[nb_iovss];
    }
    
    // Send all iovs in one chunk, the size is the total size of iovs.
    int size = 0;
    for (int i = 0; i < iovcnt; i++) {
        const iovec* data_iov = iov + i;
        size += data_iov->iov_len;
    }
    written += size;
    
    // chunk header
    int nb_size = snprintf(header_cache, SRS_HTTP_HEADER_CACHE_SIZE, "%x", size);
    if (nb_size <= 0 || nb_size >= SRS_HTTP_HEADER_CACHE_SIZE) {
        return srs_error_new(ERROR_HTTP_CONTENT_LENGTH, "overflow size=%d, expect=%d", size, nb_size);
    }
    iovss[0].iov_base = (char*)header_cache;
    iovss[0].iov_len = (int)nb_size;

    // chunk header eof.
    iovss[1].iov_base = (char*)SRS_HTTP_CRLF;
    iovss[1].iov_len = 2;

    // chunk body.
    for (int i = 0; i < iovcnt; i++) {
        iovss[2+i].iov_base = (char*)iov[i].iov_base;
        iovss[2+i].iov_len = (int)iov[i].iov_len;
    }
    
    // chunk body eof.
    iovss[2+iovcnt].iov_base = (char*)SRS_HTTP_CRLF;
    iovss[2+iovcnt].iov_len = 2;

    // sendout all ioves.
    ssize_t nwrite = 0;
    if ((err = srs_write_large_iovs(skt, iovss, nb_iovss, &nwrite)) != srs_success) {
        return srs_error_wrap(err, "writev large iovs");
    }
    
    if (pnwrite) {
        *pnwrite = nwrite;
    }
    
    return err;     
}

SrsHttpHeader* SrsHttpResponseWriter::header()
{
    return hdr;
}

void SrsHttpResponseWriter::write_header(int code)
{
    if (header_wrote) {
        srs_warn("http: multiple write_header calls, code=%d", code);
        return;
    }
    
    header_wrote = true;
    status = code;
    
    // parse the content length from header.
    content_length = hdr->content_length();
}

srs_error_t SrsHttpResponseWriter::send_header(char* data, int size)
{
    srs_error_t err = srs_success;
    
    if (header_sent) {
        return err;
    }
    header_sent = true;

    std::stringstream ss;
    
    // status_line
    ss << "HTTP/1.1 " << status << " "
    << srs_generate_http_status_text(status) << SRS_HTTP_CRLF;
    
    // detect content type
    if (srs_go_http_body_allowd(status)) {
        if (data && hdr->content_type().empty()) {
            hdr->set_content_type(srs_go_http_detect(data, size));
        }
    }
    // set server if not set.
    if (hdr->get("Server").empty()) {
        hdr->set("Server", RTMP_SIG_SRS_SERVER);
    }
    // chunked encoding
    if (content_length == -1) {
        hdr->set("Transfer-Encoding", "chunked");
    } 
    // keep alive to make vlc happy.
    if (hdr->get("Connection").empty()) {
        hdr->set("Connection", "Keep-Alive");
    }
    // Filter the header before writing it.
    if (hf && ((err = hf->filter(hdr)) != srs_success)) {
        return srs_error_wrap(err, "filter header");
    }

    // write header
    hdr->write(ss);
    
    // header_eof
    ss << SRS_HTTP_CRLF;  
    std::string buf = ss.str();
    return skt->write((void*)buf.c_str(), buf.length(), NULL);           
}

ISrsHttpHeaderFilter::ISrsHttpHeaderFilter()
{
}

ISrsHttpHeaderFilter::~ISrsHttpHeaderFilter()
{
}