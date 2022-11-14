//
// Copyright (c) 2013-2022 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//
#include <srs_utest_protocol.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_utility.hpp>
#include <string.h>

using std::string;

MockBufferIO::MockBufferIO()
{
    rtm = stm = SRS_UTIME_NO_TIMEOUT;
    rbytes = sbytes = 0;
    in_err = out_err = srs_success;
}

MockBufferIO::~MockBufferIO()
{
}

int MockBufferIO::length()
{
    return in_buffer.length();
}

MockBufferIO* MockBufferIO::append(string data)
{
    in_buffer.append((char*)data.data(), data.length());
    return this;
}

MockBufferIO* MockBufferIO::append(MockBufferIO* data)
{
    in_buffer.append(&data->in_buffer);
    return this;
}

MockBufferIO* MockBufferIO::append(uint8_t* data, int size)
{
    in_buffer.append((char*)data, size);
    return this;
}

int MockBufferIO::out_length()
{
    return out_buffer.length();
}

MockBufferIO* MockBufferIO::out_append(string data)
{
    out_buffer.append((char*)data.data(), data.length());
    return this;
}

MockBufferIO* MockBufferIO::out_append(MockBufferIO* data)
{
    out_buffer.append(&data->out_buffer);
    return this;
}

MockBufferIO* MockBufferIO::out_append(uint8_t* data, int size)
{
    out_buffer.append((char*)data, size);
    return this;
}

srs_error_t MockBufferIO::read_fully(void* buf, size_t size, ssize_t* nread)
{
    if (in_err != srs_success) {
        return srs_error_copy(in_err);
    }

    if (in_buffer.length() < (int)size) {
        return srs_error_new(ERROR_SOCKET_READ, "read");
    }
    memcpy(buf, in_buffer.bytes(), size);
    
    rbytes += size;
    if (nread) {
        *nread = size;
    }
    in_buffer.erase(size);
    return srs_success;
}

srs_error_t MockBufferIO::write(void* buf, size_t size, ssize_t* nwrite)
{
    if (out_err != srs_success) {
        return srs_error_copy(out_err);
    }

    sbytes += size;
    if (nwrite) {
        *nwrite = size;
    }
    out_buffer.append((char*)buf, size);
    return srs_success;
}

void MockBufferIO::set_recv_timeout(srs_utime_t tm)
{
    rtm = tm;
}

srs_utime_t MockBufferIO::get_recv_timeout()
{
    return rtm;
}

int64_t MockBufferIO::get_recv_bytes()
{
    return rbytes;
}

void MockBufferIO::set_send_timeout(srs_utime_t tm)
{
    stm = tm;
}

srs_utime_t MockBufferIO::get_send_timeout()
{
    return stm;
}

int64_t MockBufferIO::get_send_bytes()
{
    return sbytes;
}

srs_error_t MockBufferIO::writev(const iovec *iov, int iov_size, ssize_t* nwrite)
{
    srs_error_t err = srs_success;

    if (out_err != srs_success) {
        return srs_error_copy(out_err);
    }
    
    ssize_t total = 0;
    for (int i = 0; i <iov_size; i++) {
        const iovec& pi = iov[i];
        
        ssize_t writen = 0;
        if ((err = write(pi.iov_base, pi.iov_len, &writen)) != srs_success) {
            return err;
        }
        total += writen;
    }
    
    if (nwrite) {
        *nwrite = total;
    }
    return err;
}

srs_error_t MockBufferIO::read(void* buf, size_t size, ssize_t* nread)
{
    if (in_err != srs_success) {
        return srs_error_copy(in_err);
    }

    if (in_buffer.length() <= 0) {
        return srs_error_new(ERROR_SOCKET_READ, "read");
    }
    
    size_t available = srs_min(in_buffer.length(), (int)size);
    memcpy(buf, in_buffer.bytes(), available);
    
    rbytes += available;
    if (nread) {
        *nread = available;
    }
    in_buffer.erase(available);

    return srs_success;
}

int MockBufferIO::get_fd()
{

}