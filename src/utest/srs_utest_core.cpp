#include <srs_utest_core.hpp>

#include <srs_core_auto_free.hpp>

VOID TEST(CoreAutoFreeTest, Free)
{
    char* data = new char[32];
    srs_freepa(data);
    EXPECT_TRUE(data == NULL);

    if (true) {
        data = new char[32];
        SrsAutoFreeA(char, data);
    }
    EXPECT_TRUE(data == NULL);
}