#include "minitest.h"
#include "tinysprite/recent_colors.h"

static Rgba shade(int value)
{
    return (Rgba){(uint8_t)value, 0, 0, 255};
}

TEST(starts_empty)
{
    RecentColors recent = {0};
    ASSERT_EQ_INT(0, recent.count);
}

TEST(newest_color_comes_first)
{
    RecentColors recent = {0};
    recent_colors_push(&recent, shade(1));
    recent_colors_push(&recent, shade(2));
    ASSERT_EQ_INT(2, recent.count);
    ASSERT_EQ_INT(2, recent.colors[0].r);
    ASSERT_EQ_INT(1, recent.colors[1].r);
}

TEST(pushing_a_known_color_moves_it_to_the_front)
{
    RecentColors recent = {0};
    recent_colors_push(&recent, shade(1));
    recent_colors_push(&recent, shade(2));
    recent_colors_push(&recent, shade(3));
    recent_colors_push(&recent, shade(1));
    ASSERT_EQ_INT(3, recent.count);
    ASSERT_EQ_INT(1, recent.colors[0].r);
    ASSERT_EQ_INT(3, recent.colors[1].r);
    ASSERT_EQ_INT(2, recent.colors[2].r);
    recent_colors_push(&recent, shade(1)); /* already first: nothing moves */
    ASSERT_EQ_INT(3, recent.count);
    ASSERT_EQ_INT(1, recent.colors[0].r);
    ASSERT_EQ_INT(3, recent.colors[1].r);
}

TEST(alpha_makes_a_different_color)
{
    RecentColors recent = {0};
    recent_colors_push(&recent, shade(1));
    recent_colors_push(&recent, (Rgba){1, 0, 0, 100});
    ASSERT_EQ_INT(2, recent.count);
}

TEST(the_oldest_color_drops_when_full)
{
    RecentColors recent = {0};
    for (int i = 1; i <= RECENT_COLORS_MAX + 3; i++) {
        recent_colors_push(&recent, shade(i));
    }
    ASSERT_EQ_INT(RECENT_COLORS_MAX, recent.count);
    ASSERT_EQ_INT(RECENT_COLORS_MAX + 3, recent.colors[0].r);
    ASSERT_EQ_INT(4, recent.colors[RECENT_COLORS_MAX - 1].r);
}

TEST(null_is_ignored)
{
    recent_colors_push(NULL, shade(1));
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN(starts_empty);
    RUN(newest_color_comes_first);
    RUN(pushing_a_known_color_moves_it_to_the_front);
    RUN(alpha_makes_a_different_color);
    RUN(the_oldest_color_drops_when_full);
    RUN(null_is_ignored);
    return TEST_SUMMARY();
}
