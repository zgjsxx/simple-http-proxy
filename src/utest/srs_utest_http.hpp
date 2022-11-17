//
// Copyright (c) 2013-2022 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//

#ifndef SRS_UTEST_PROTO_STACK_HPP
#define SRS_UTEST_PROTO_STACK_HPP

#include <srs_utest_main.hpp>

#include <srs_protocol_http_stack.hpp>
#include <srs_protocol_http_conn.hpp>
#include <srs_utest_protocol.hpp>

class MockResponseWriter : public ISrsHttpResponseWriter, public ISrsHttpHeaderFilter
{
public:
    SrsHttpResponseWriter* w;
    MockBufferIO io;
public:
    MockResponseWriter();
    virtual ~MockResponseWriter();
public:
    virtual srs_error_t final_request();
    virtual SrsHttpHeader* header();
    virtual srs_error_t write(char* data, int size);
    virtual srs_error_t writev(const iovec* iov, int iovcnt, ssize_t* pnwrite);
    virtual void write_header(int code);
public:
    virtual srs_error_t filter(SrsHttpHeader* h);
};

class MockMSegmentsReader : public ISrsReader
{
public:
    vector<string> in_bytes;
public:
    MockMSegmentsReader();
    virtual ~MockMSegmentsReader();
public:
    virtual void append(string b) {
        in_bytes.push_back(b);
    }
    virtual srs_error_t read(void* buf, size_t size, ssize_t* nread);
};

string mock_http_response(int status, string content);
string mock_http_response2(int status, string content);
bool is_string_contain(string substr, string str);

#define __MOCK_HTTP_EXPECT_STREQ(status, text, w) \
        EXPECT_STREQ(mock_http_response(status, text).c_str(), HELPER_BUFFER2STR(&w.io.out_buffer).c_str())

#endif