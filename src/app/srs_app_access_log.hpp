//
// Copyright (c) 2013-2022 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//

#ifndef SRS_APP_ACCESS_LOG_HPP
#define SRS_APP_ACCESS_LOG_HPP

#include <srs_kernel_file.hpp>
#include <string>
using std::string;
class SrsAccessLogInfo
{
public:
    string domain;
    string client_ip;
    int status_code;
};

class SrsAccessLog : public SrsFileWriter
{
public:
    SrsAccessLog();
    ~SrsAccessLog();
public:
    virtual void write_access_log(SrsAccessLogInfo* info);
private:
    virtual void init();
};

#endif