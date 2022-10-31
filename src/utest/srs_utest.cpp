#include <srs_kernel_log.hpp>
#include <srs_kernel_error.hpp>
#include <srs_app_config.hpp>
#include <srs_utest.hpp>

ISrsLog* _srs_log = NULL;
ISrsContext* _srs_context = NULL;
// app module.
SrsConfig* _srs_config = NULL;

GTEST_API_ int main(int argc, char **argv) {
    srs_error_t err = srs_success;

    // if ((err = prepare_main()) != srs_success) {
    //     fprintf(stderr, "Failed, %s\n", srs_error_desc(err).c_str());

    //     int ret = srs_error_code(err);
    //     srs_freep(err);
    //     return ret;
    // }

    printf("start to test\n");

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}