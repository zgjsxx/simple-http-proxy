//
// Copyright (c) 2013-2022 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//

#ifndef SRS_UTEST_CONFIG_HPP
#define SRS_UTEST_CONFIG_HPP

#include <srs_core.hpp>
#include <srs_utest_main.hpp>
#include <srs_app_access_log.hpp>
#include <string>

using std::string;
class MockSrsAccessLog : public  SrsAccessLog
{
private:
    string str;
public:
    virtual srs_error_t open(string p);
    virtual srs_error_t write(void* buf, size_t count, ssize_t* pnwrite);
public:
    string getStr();
};
#endif