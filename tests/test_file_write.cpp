#include <cstdio>
#include <filesystem>
#include <optional>
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

TEST(a_utf8_path_round_trips_and_a_file_there_can_be_written_and_read)
{
    const std::string folder = "test_file_write_\xed\x95\x9c\xea\xb8\x80";
    const std::string path = folder + "/\xed\x8c\x8c\xec\x9d\xbc.pxa";
    ASSERT_TRUE(path_to_utf8(path_from_utf8(path)) == path);
    std::filesystem::create_directories(path_from_utf8(folder));
    Sprite sprite(3, 2);
    ASSERT_TRUE(pxa_save(sprite, path));
    std::optional<Sprite> back = pxa_load(path);
    ASSERT_TRUE(back.has_value());
    ASSERT_EQ_INT(3, back->width());
    std::filesystem::remove_all(path_from_utf8(folder));
}

int main()
{
    RUN(a_failed_write_says_why);
    RUN(a_successful_write_clears_the_reason);
    RUN(a_utf8_path_round_trips_and_a_file_there_can_be_written_and_read);
    return TEST_SUMMARY();
}
