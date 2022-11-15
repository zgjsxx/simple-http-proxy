#include <srs_kernel_log.hpp>
#include <srs_kernel_error.hpp>
#include <srs_app_config.hpp>
#include <srs_utest_main.hpp>
#include <srs_app_policy.hpp>

ISrsLog* _srs_log = NULL;
ISrsContext* _srs_context = NULL;
SrsConfig* _srs_config = NULL;
// @global policy object for app module
SrsPolicy* _srs_policy = NULL;
// @global notification object for app module
SrsNotification* _srs_notification = NULL;

// Initialize global settings.
srs_error_t prepare_main() {
    srs_error_t err = srs_success;
    
    if ((err = srs_global_initialize()) != srs_success) {
        return srs_error_wrap(err, "init global");
    }

    srs_freep(_srs_log);
    _srs_log = new MockEmptyLog(SrsLogLevelDisabled);
    
    return err;
}

GTEST_API_ int main(int argc, char **argv) {
    srs_error_t err = srs_success;

    if ((err = prepare_main()) != srs_success) {
        fprintf(stderr, "Failed, %s\n", srs_error_desc(err).c_str());

        int ret = srs_error_code(err);
        srs_freep(err);
        return ret;
    }

    srs_info("start all test");

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


MockEmptyLog::MockEmptyLog(SrsLogLevel l)
{
    level = l;
}

MockEmptyLog::~MockEmptyLog()
{
}


