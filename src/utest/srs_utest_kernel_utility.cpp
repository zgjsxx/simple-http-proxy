#include "srs_utest_kernel_utility.hpp"
#include "srs_kernel_utility.hpp"

VOID TEST(srs_string_replace_test, replace_test1)
{
    string temp = "url: %URL%";
    string res = srs_string_replace(temp, "%URL%", "example.com");
    EXPECT_TRUE(res == "url: example.com");
}