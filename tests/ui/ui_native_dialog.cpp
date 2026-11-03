/* The system file window: the filter SDL is given for the extensions a dialog lists, and how the
 * answer that arrives later (possibly on another thread) is kept until a frame takes it. The window
 * itself needs a real display, so it is not opened here. */
#include <string>

#include "minitest.hpp"
#include "native_dialog.hpp"

TEST(extensions_become_the_semicolon_list_sdl_wants)
{
    ASSERT_TRUE(native_dialog_patterns(".png .gif") == "png;gif");
    ASSERT_TRUE(native_dialog_patterns(".pxa") == "pxa");
    ASSERT_TRUE(native_dialog_patterns("  .png   .jpg ") == "png;jpg");
    ASSERT_TRUE(native_dialog_patterns("png") == "png");
}

TEST(no_extensions_mean_no_filter)
{
    ASSERT_TRUE(native_dialog_patterns("").empty());
    ASSERT_TRUE(native_dialog_patterns("   ").empty());
}

TEST(an_entry_with_a_character_that_is_special_to_the_dialog_is_dropped)
{
    ASSERT_TRUE(native_dialog_patterns(".png .*  .a;b .gif") == "png;gif");
    ASSERT_TRUE(native_dialog_patterns(".*").empty());
}

TEST(a_chosen_file_is_kept_until_it_is_taken_once)
{
    const char *const files[] = {"/art/a.pxa", nullptr};
    native_dialog_finish(files);
    const std::optional<std::string> first = native_dialog_take();
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(first && *first == "/art/a.pxa");
    ASSERT_TRUE(!native_dialog_take().has_value());
}

TEST(a_cancelled_window_and_a_failed_one_leave_no_answer)
{
    const char *const none[] = {nullptr};
    native_dialog_finish(none); /* the user closed it */
    ASSERT_TRUE(!native_dialog_take().has_value());
    native_dialog_finish(nullptr); /* the system could not show it */
    ASSERT_TRUE(!native_dialog_take().has_value());
    ASSERT_TRUE(!native_dialog_open());
}

TEST(one_window_is_open_at_a_time_until_it_answers_or_is_closed)
{
    ASSERT_TRUE(!native_dialog_open());
    ASSERT_TRUE(native_dialog_claim("/art", ".pxa"));
    ASSERT_TRUE(native_dialog_open());
    ASSERT_TRUE(!native_dialog_claim("/other", ".png")); /* a second one is refused */
    const char *const none[] = {nullptr};
    native_dialog_finish(none); /* closed without a choice */
    ASSERT_TRUE(!native_dialog_open());
    ASSERT_TRUE(native_dialog_claim("/art", ""));
    native_dialog_finish(nullptr); /* the system could not show it */
    ASSERT_TRUE(!native_dialog_open());
    ASSERT_TRUE(native_dialog_claim("/art", ""));
    const char *const chosen[] = {"/art/a.pxa", nullptr};
    native_dialog_finish(chosen);
    ASSERT_TRUE(!native_dialog_open());
    native_dialog_take();
}

TEST(only_the_first_of_several_chosen_files_counts)
{
    const char *const files[] = {"/art/a.pxa", "/art/b.pxa", nullptr};
    native_dialog_finish(files);
    const std::optional<std::string> first = native_dialog_take();
    ASSERT_TRUE(first && *first == "/art/a.pxa");
}

TEST(a_newer_answer_replaces_one_that_was_never_taken)
{
    const char *const a[] = {"/art/a.pxa", nullptr};
    const char *const b[] = {"/art/b.pxa", nullptr};
    native_dialog_finish(a);
    native_dialog_finish(b);
    const std::optional<std::string> got = native_dialog_take();
    ASSERT_TRUE(got && *got == "/art/b.pxa");
}

int main()
{
    RUN(extensions_become_the_semicolon_list_sdl_wants);
    RUN(no_extensions_mean_no_filter);
    RUN(an_entry_with_a_character_that_is_special_to_the_dialog_is_dropped);
    RUN(a_chosen_file_is_kept_until_it_is_taken_once);
    RUN(a_cancelled_window_and_a_failed_one_leave_no_answer);
    RUN(one_window_is_open_at_a_time_until_it_answers_or_is_closed);
    RUN(only_the_first_of_several_chosen_files_counts);
    RUN(a_newer_answer_replaces_one_that_was_never_taken);
    return TEST_SUMMARY();
}
