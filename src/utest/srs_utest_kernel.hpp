//
// Copyright (c) 2013-2022 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//

#ifndef SRS_UTEST_KERNEL_HPP
#define SRS_UTEST_KERNEL_HPP

#include <srs_core.hpp>
#include <srs_kernel_io.hpp>
#include <string>
#include <srs_utest_main.hpp>
using namespace std;

class MockBufferReader: public ISrsReader
{
private:
    std::string str;
public:
    MockBufferReader(const char* data);
    virtual ~MockBufferReader();
public:
    virtual srs_error_t read(void* buf, size_t size, ssize_t* nread);
};

#endif