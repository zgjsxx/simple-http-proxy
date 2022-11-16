//
// Copyright (c) 2013-2022 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//
#include <srs_utest_config.hpp>
#include <srs_kernel_error.hpp>

MockSrsConfigBuffer::MockSrsConfigBuffer(string buf)
{
    // read all.
    int filesize = (int)buf.length();
    
    if (filesize <= 0) {
        return;
    }
    
    // create buffer
    pos = last = start = new char[filesize];
    end = start + filesize;
    
    memcpy(start, buf.data(), filesize);
}

MockSrsConfigBuffer::~MockSrsConfigBuffer()
{
}

srs_error_t MockSrsConfigBuffer::fullfill(const char* /*filename*/)
{
    return srs_success;
}

MockSrsConfig::MockSrsConfig()
{
}

MockSrsConfig::~MockSrsConfig()
{
}

srs_error_t MockSrsConfig::parse(string buf)
{
    srs_error_t err = srs_success;

    MockSrsConfigBuffer buffer(buf);

    if ((err = parse_buffer(&buffer)) != srs_success) {
        return srs_error_wrap(err, "parse buffer");
    }
    
    // if ((err = srs_config_transform_vhost(root)) != srs_success) {
    //     return srs_error_wrap(err, "transform config");
    // }

    // if ((err = check_normal_config()) != srs_success) {
    //     return srs_error_wrap(err, "check normal config");
    // }
    
    return err;
}

srs_error_t MockSrsConfig::mock_include(const string file_name, const string content)
{
    srs_error_t err = srs_success;

    included_files[file_name] = content;

    return err;
}

srs_error_t MockSrsConfig::build_buffer(std::string src, srs_internal::SrsConfigBuffer** pbuffer)
{
    srs_error_t err = srs_success;

    string content = included_files[src];
    if(content.empty()) {
        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "file %s: no found", src.c_str());
    }

    *pbuffer = new MockSrsConfigBuffer(content);

    return err;
}