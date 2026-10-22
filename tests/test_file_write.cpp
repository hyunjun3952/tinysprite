#include <cstdio>
#include <string>

#include "minitest.hpp"
#include "tinysprite/io.hpp"

TEST(a_failed_write_says_why)
{
    ASSERT_TRUE(!file_write_atomic("/nonexistent-tinysprite-dir/file.bin", "x"));
    ASSERT_TRUE(!file_write_error().empty());
}

TEST(a_successful_write_clears_the_reason)
{
    file_write_atomic("/nonexistent-tinysprite-dir/file.bin", "x");
    ASSERT_TRUE(file_write_atomic("test_file_write.tmpout", "x"));
    ASSERT_TRUE(file_write_error().empty());
    std::remove("test_file_write.tmpout");
}

int main()
{
    RUN(a_failed_write_says_why);
    RUN(a_successful_write_clears_the_reason);
    return TEST_SUMMARY();
}
