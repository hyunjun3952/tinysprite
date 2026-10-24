/* The window size the app opens with is kept inside the part of the display a window can use, so a
 * small screen or a large UI scale never opens a window whose edges are off the screen. */
#include "minitest.hpp"
#include "window_size.hpp"

TEST(a_window_that_fits_is_left_alone)
{
    WindowSize size = window_fit(WindowSize{1100, 720}, WindowSize{1920, 1040});
    ASSERT_EQ_INT(1100, size.width);
    ASSERT_EQ_INT(720, size.height);
}

TEST(a_window_larger_than_the_screen_shrinks_to_it)
{
    WindowSize size = window_fit(WindowSize{1100, 720}, WindowSize{1366, 680});
    ASSERT_EQ_INT(1100, size.width);
    ASSERT_EQ_INT(680 - WINDOW_FRAME_MARGIN, size.height);
    size = window_fit(WindowSize{1650, 1080}, WindowSize{1280, 720});
    ASSERT_EQ_INT(1280 - WINDOW_FRAME_MARGIN, size.width);
    ASSERT_EQ_INT(720 - WINDOW_FRAME_MARGIN, size.height);
}

TEST(an_unknown_screen_changes_nothing)
{
    WindowSize size = window_fit(WindowSize{1100, 720}, WindowSize{0, 0});
    ASSERT_EQ_INT(1100, size.width);
    ASSERT_EQ_INT(720, size.height);
}

TEST(the_minimum_size_scales_with_the_ui)
{
    WindowSize one = window_minimum(1.0f);
    WindowSize two = window_minimum(2.0f);
    ASSERT_TRUE(one.width > 0 && one.height > 0);
    ASSERT_EQ_INT(one.width * 2, two.width);
    ASSERT_EQ_INT(one.height * 2, two.height);
}

TEST(the_largest_scale_is_the_one_whose_minimum_window_still_fits_the_screen)
{
    /* 1920 x 1040 usable: the height decides, (1040 - frame margin) / 420 */
    float scale = window_max_scale(WindowSize{1920, 1040});
    WindowSize at = window_minimum(scale);
    ASSERT_TRUE(at.width <= 1920 - WINDOW_FRAME_MARGIN);
    ASSERT_TRUE(at.height <= 1040 - WINDOW_FRAME_MARGIN);
    WindowSize over = window_minimum(scale + 0.05f);
    ASSERT_TRUE(over.width > 1920 - WINDOW_FRAME_MARGIN ||
                over.height > 1040 - WINDOW_FRAME_MARGIN);
}

TEST(a_screen_smaller_than_the_minimum_still_allows_scale_one)
{
    ASSERT_TRUE(window_max_scale(WindowSize{500, 300}) == 1.0f);
}

TEST(an_unknown_screen_allows_every_scale)
{
    ASSERT_TRUE(window_max_scale(WindowSize{0, 0}) >= 3.0f);
}

int main()
{
    RUN(a_window_that_fits_is_left_alone);
    RUN(a_window_larger_than_the_screen_shrinks_to_it);
    RUN(an_unknown_screen_changes_nothing);
    RUN(the_minimum_size_scales_with_the_ui);
    RUN(the_largest_scale_is_the_one_whose_minimum_window_still_fits_the_screen);
    RUN(a_screen_smaller_than_the_minimum_still_allows_scale_one);
    RUN(an_unknown_screen_allows_every_scale);
    return TEST_SUMMARY();
}
