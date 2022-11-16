//
// Copyright (c) 2013-2022 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//
#include <srs_utest_kernel.hpp>
#include <srs_kernel_error.hpp>
#include <srs_protocol_stream.hpp>
#include <srs_kernel_utility.hpp>
#include <string.h>

MockBufferReader::MockBufferReader(const char* data)
{
    str = data;
}

MockBufferReader::~MockBufferReader()
{
}

srs_error_t MockBufferReader::read(void* buf, size_t size, ssize_t* nread)
{
    if (str.empty()) {
        return srs_error_new(ERROR_SYSTEM_FILE_EOF, "EOF");
    }

    int len = srs_min(str.length(), size);
    if (len == 0) {
        return srs_error_new(-1, "no data");
    }

    memcpy(buf, str.data(), len);
    str = str.substr(len);
    
    if (nread) {
        *nread = len;
    }

    return srs_success;
}


VOID TEST(KernelFastBufferTest, Grow1)
{
    srs_error_t err;

    if(true) {
        SrsFastStream b(5);
        MockBufferReader r("Hello, world!");

        HELPER_ASSERT_SUCCESS(b.grow(&r, 5));
        b.skip(2);

        HELPER_ASSERT_FAILED(b.grow(&r, 6));
    } 
       
}

VOID TEST(KernelFastBufferTest, Grow2)
{
    srs_error_t err;

    if(true) {
        SrsFastStream b(6);
        MockBufferReader r("Hello, world!");

        HELPER_ASSERT_SUCCESS(b.grow(&r, 5));
        EXPECT_EQ('H', b.read_1byte()); EXPECT_EQ('e', b.read_1byte()); EXPECT_EQ('l', b.read_1byte());
        b.skip(2);

        HELPER_ASSERT_SUCCESS(b.grow(&r, 2));
        b.skip(2);

        HELPER_ASSERT_SUCCESS(b.grow(&r, 5));
        EXPECT_EQ('w', b.read_1byte()); EXPECT_EQ('o', b.read_1byte()); EXPECT_EQ('r', b.read_1byte());
        b.skip(2);
    } 
       
}

