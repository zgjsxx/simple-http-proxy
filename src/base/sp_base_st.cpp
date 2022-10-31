// SPDX-License-Identifier: MIT
//
// Copyright (C) 2021-2022 zgjsxx

#include <st.h>

#include <sp_base_st.hpp>

void srs_st_init()
{
    //TODO compare srs_protocol_st.cpp, need to add platform code
    int r0 = 0;
    if((r0 = st_init()) != 0){
        // return srs_error_new(ERROR_ST_INITIALIZE, "st initialize failed, r0=%d", r0);
    }
}

int srs_key_create(int *keyp, void (*destructor)(void *))
{
    return st_key_create(keyp, destructor);
}

void *srs_thread_getspecific(int key)
{
    return st_thread_getspecific(key);
}

int srs_thread_setspecific(int key, void *value)
{
    return st_thread_setspecific(key, value);
}

// int srs_thread_join(srs_thread_t thread, void **retvalp)
// {
//     return st_thread_join((st_thread_t)thread, retvalp);
// }

// void srs_thread_interrupt(srs_thread_t thread)
// {
//     st_thread_interrupt((st_thread_t)thread);
// }