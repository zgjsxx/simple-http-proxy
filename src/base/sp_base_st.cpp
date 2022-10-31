// SPDX-License-Identifier: MIT
//
// Copyright (C) 2021-2022 zgjsxx

#include <st.h>

#include <sp_base_st.hpp>

void srs_st_init()
{
    int r0 = 0;
    if((r0 = st_init()) != 0){
        // return srs_error_new(ERROR_ST_INITIALIZE, "st initialize failed, r0=%d", r0);
    }
}