#ifndef SRS_UTEST_PUBLIC_SHARED_HPP
#define SRS_UTEST_PUBLIC_SHARED_HPP

#include "gtest/gtest.h"
#include <string>
#include <srs_core.hpp>
using namespace std;

// we add an empty macro for upp to show the smart tips.
#define VOID

// For errors.
// @remark we directly delete the err, because we allow user to append message if fail.
#define HELPER_EXPECT_SUCCESS(x) \
    if ((err = x) != srs_success) fprintf(stderr, "err %s", srs_error_desc(err).c_str()); \
    if (err != srs_success) delete err; \
    EXPECT_TRUE(srs_success == err)
#define HELPER_EXPECT_FAILED(x) \
    if ((err = x) != srs_success) delete err; \
    EXPECT_TRUE(srs_success != err)

// Dump simple stream to string.
#define HELPER_BUFFER2STR(io) \
    string((const char*)(io)->bytes(), (size_t)(io)->length())

#endif