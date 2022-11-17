#include <srs_utest_app_access_log.hpp>
#include <srs_core_auto_free.hpp>

srs_error_t MockSrsAccessLog::open(string p)
{
    srs_error_t err = srs_success;
    return err;
}

srs_error_t MockSrsAccessLog::write(void* buf, size_t count, ssize_t* pnwrite)
{
    srs_error_t err = srs_success;
    str.append((const char*)buf, count);
    return err;
}

string MockSrsAccessLog::getStr()
{
    return str;
}

VOID TEST(SrsAccessLog, test1)
{
    SrsAccessLogInfo* log_info = new SrsAccessLogInfo();
    SrsAutoFree(SrsAccessLogInfo, log_info);
    log_info->client_ip = "1.1.1.1";
    log_info->domain  = "example.com";
    log_info->status_code = 200;
    MockSrsAccessLog mockSrsAccessLog;
    mockSrsAccessLog.write_access_log(log_info);
    EXPECT_EQ("1.1.1.1\texample.com\t200\r\n", mockSrsAccessLog.getStr());
}
