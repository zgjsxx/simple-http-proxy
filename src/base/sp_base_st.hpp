// SPDX-License-Identifier: MIT
//
// Copyright (C) 2021-2022 zgjsxx



// Initialize st, requires epoll.
extern void srs_st_init();

extern int srs_thread_setspecific(int key, void *value);
extern int srs_key_create(int *keyp, void (*destructor)(void *));
extern void *srs_thread_getspecific(int key);

// extern int srs_thread_join(srs_thread_t thread, void **retvalp);
// extern void srs_thread_interrupt(srs_thread_t thread);