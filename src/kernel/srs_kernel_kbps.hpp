//
// Copyright (c) 2013-2022 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//

#ifndef SRS_KERNEL_KBPS_HPP
#define SRS_KERNEL_KBPS_HPP

#include <srs_core.hpp>

class SrsWallClock
{
public:
    SrsWallClock();
    virtual ~SrsWallClock();
public:
    /**
     * Current time in srs_utime_t.
     */
    virtual srs_utime_t now();
};


#endif