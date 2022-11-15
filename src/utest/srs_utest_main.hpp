#ifndef SRS_UTEST_PUBLIC_SHARED_HPP
#define SRS_UTEST_PUBLIC_SHARED_HPP

#include "gtest/gtest.h"
#include <string>
#include <srs_core.hpp>
#include <srs_app_log.hpp>
#include <srs_kernel_log.hpp>
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

// For errors, assert.
// @remark we directly delete the err, because we allow user to append message if fail.
#define HELPER_ASSERT_SUCCESS(x) \
    if ((err = x) != srs_success) fprintf(stderr, "err %s", srs_error_desc(err).c_str()); \
    if (err != srs_success) delete err; \
    ASSERT_TRUE(srs_success == err)
#define HELPER_ASSERT_FAILED(x) \
    if ((err = x) != srs_success) delete err; \
    ASSERT_TRUE(srs_success != err)

// For init array data.
#define HELPER_ARRAY_INIT(buf, sz, val) \
    for (int _iii = 0; _iii < (int)sz; _iii++) (buf)[_iii] = val

// Dump simple stream to string.
#define HELPER_BUFFER2STR(io) \
    string((const char*)(io)->bytes(), (size_t)(io)->length())

// Covert uint8_t array to string.
#define HELPER_ARR2STR(arr, size) \
    string((char*)(arr), (int)size)

class MockEmptyLog : public SrsFileLog
{
public:
    MockEmptyLog(SrsLogLevel l);
    virtual ~MockEmptyLog();
};

#endif