//
// Copyright (c) 2013-2022 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//

#include <srs_protocol_kbps.hpp>
#include <srs_protocol_io.hpp>

ISrsKbpsDelta::ISrsKbpsDelta()
{
}

ISrsKbpsDelta::~ISrsKbpsDelta()
{
}

SrsNetworkDelta::SrsNetworkDelta()
{
    in_ = out_ = NULL;
    in_base_ = in_delta_ = 0;
    out_base_ = out_delta_ = 0;
}

SrsNetworkDelta::~SrsNetworkDelta()
{
}

void SrsNetworkDelta::set_io(ISrsProtocolStatistic* in, ISrsProtocolStatistic* out)
{
    if (in_) {
        in_delta_ += in_->get_recv_bytes() - in_base_;
    }
    if (in) {
        in_base_ = in->get_recv_bytes();
        in_delta_ += in_base_;
    }
    in_ = in;

    if (out_) {
        out_delta_ += out_->get_send_bytes() - out_base_;
    }
    if (out) {
        out_base_ = out->get_send_bytes();
        out_delta_ += out_base_;
    }
    out_ = out;
}

void SrsNetworkDelta::remark(int64_t* in, int64_t* out)
{
    if (in_) {
        in_delta_ += in_->get_recv_bytes() - in_base_;
        in_base_ = in_->get_recv_bytes();
    }
    if (out_) {
        out_delta_ += out_->get_send_bytes() - out_base_;
        out_base_ = out_->get_send_bytes();
    }

    *in = in_delta_;
    *out = out_delta_;
    in_delta_ = out_delta_ = 0;
}