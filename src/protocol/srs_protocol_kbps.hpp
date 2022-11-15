//
// Copyright (c) 2013-2022 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//

#ifndef SRS_PROTOCOL_KBPS_HPP
#define SRS_PROTOCOL_KBPS_HPP

//related with kbps calculation
#include <srs_core.hpp>

class ISrsProtocolStatistic;

class ISrsKbpsDelta
{
public:
    ISrsKbpsDelta();
    virtual ~ISrsKbpsDelta();
public:
    // Resample to get the value of delta bytes.
    // @remark If no delta bytes, both in and out will be set to 0.
    virtual void remark(int64_t* in, int64_t* out) = 0;
};

// A network delta data source for SrsKbps.
class SrsNetworkDelta : public ISrsKbpsDelta
{
private:
    ISrsProtocolStatistic* in_;
    ISrsProtocolStatistic* out_;
    uint64_t in_base_;
    uint64_t in_delta_;
    uint64_t out_base_;
    uint64_t out_delta_;
public:
    SrsNetworkDelta();
    virtual ~SrsNetworkDelta();
public:
    // Switch the under-layer network io, we use the bytes as a fresh delta.
    virtual void set_io(ISrsProtocolStatistic* in, ISrsProtocolStatistic* out);
// Interface ISrsKbpsDelta.
public:
    virtual void remark(int64_t* in, int64_t* out);
};



#endif