#include "minitest.h"
#include "tinysprite/palette.h"

TEST(create_is_empty)
{
    Palette p;
    ASSERT_EQ_INT(0, p.count());
}

TEST(add_returns_sequential_indices)
{
    Palette p;
    Rgba red = {255, 0, 0, 255};
    Rgba blue = {0, 0, 255, 255};
    ASSERT_EQ_INT(0, p.add(red));
    ASSERT_EQ_INT(1, p.add(blue));
    ASSERT_EQ_INT(2, p.count());
    ASSERT_EQ_INT(255, p.get(0).r);
    ASSERT_EQ_INT(255, p.get(1).b);
}

TEST(get_out_of_range_is_transparent)
{
    Palette p;
    Rgba c = p.get(0);
    ASSERT_EQ_INT(0, c.a);
    c = p.get(-1);
    ASSERT_EQ_INT(0, c.a);
}

TEST(set_replaces_color)
{
    Palette p;
    Rgba red = {255, 0, 0, 255};
    Rgba green = {0, 255, 0, 255};
    p.add(red);
    ASSERT_TRUE(p.set(0, green));
    ASSERT_EQ_INT(255, p.get(0).g);
    ASSERT_EQ_INT(0, p.get(0).r);
    ASSERT_TRUE(!p.set(1, green));
    ASSERT_TRUE(!p.set(-1, green));
}

TEST(add_fails_when_full)
{
    Palette p;
    Rgba c = {1, 2, 3, 255};
    for (int i = 0; i < PALETTE_MAX_COLORS; i++) {
        ASSERT_EQ_INT(i, p.add(c));
    }
    ASSERT_EQ_INT(-1, p.add(c));
    ASSERT_EQ_INT(PALETTE_MAX_COLORS, p.count());
}

TEST(remove_shifts_later_colors_down)
{
    Palette p;
    p.add(Rgba{1, 0, 0, 255});
    p.add(Rgba{2, 0, 0, 255});
    p.add(Rgba{3, 0, 0, 255});
    ASSERT_TRUE(p.remove(1));
    ASSERT_EQ_INT(2, p.count());
    ASSERT_EQ_INT(1, p.get(0).r);
    ASSERT_EQ_INT(3, p.get(1).r);
    ASSERT_EQ_INT(0, p.get(2).a);
    ASSERT_TRUE(p.remove(1));
    ASSERT_TRUE(p.remove(0));
    ASSERT_EQ_INT(0, p.count());
}

TEST(remove_out_of_range_fails)
{
    Palette p;
    p.add(Rgba{1, 0, 0, 255});
    ASSERT_TRUE(!p.remove(1));
    ASSERT_TRUE(!p.remove(-1));
    ASSERT_EQ_INT(1, p.count());
}

TEST(find_matches_exact_colors)
{
    Palette p;
    p.add(Rgba{1, 2, 3, 255});
    p.add(Rgba{1, 2, 3, 128});
    ASSERT_EQ_INT(0, p.find(Rgba{1, 2, 3, 255}));
    ASSERT_EQ_INT(1, p.find(Rgba{1, 2, 3, 128}));
    ASSERT_EQ_INT(-1, p.find(Rgba{1, 2, 4, 255}));
}

TEST(rgba_equality_compares_all_four_channels)
{
    Rgba c = {1, 2, 3, 4};
    ASSERT_TRUE((c == Rgba{1, 2, 3, 4}));
    ASSERT_TRUE((c != Rgba{0, 2, 3, 4}));
    ASSERT_TRUE((c != Rgba{1, 0, 3, 4}));
    ASSERT_TRUE((c != Rgba{1, 2, 0, 4}));
    ASSERT_TRUE((c != Rgba{1, 2, 3, 0}));
}

static Rgba shade(int value)
{
    return Rgba{static_cast<uint8_t>(value), 0, 0, 255};
}

TEST(recent_starts_empty)
{
    RecentColors recent;
    ASSERT_EQ_INT(0, recent.size());
}

TEST(newest_color_comes_first)
{
    RecentColors recent;
    recent.push(shade(1));
    recent.push(shade(2));
    ASSERT_EQ_INT(2, recent.size());
    ASSERT_EQ_INT(2, recent[0].r);
    ASSERT_EQ_INT(1, recent[1].r);
}

TEST(pushing_a_known_color_moves_it_to_the_front)
{
    RecentColors recent;
    recent.push(shade(1));
    recent.push(shade(2));
    recent.push(shade(3));
    recent.push(shade(1));
    ASSERT_EQ_INT(3, recent.size());
    ASSERT_EQ_INT(1, recent[0].r);
    ASSERT_EQ_INT(3, recent[1].r);
    ASSERT_EQ_INT(2, recent[2].r);
    recent.push(shade(1)); /* already first: nothing moves */
    ASSERT_EQ_INT(3, recent.size());
    ASSERT_EQ_INT(1, recent[0].r);
    ASSERT_EQ_INT(3, recent[1].r);
}

TEST(alpha_makes_a_different_color)
{
    RecentColors recent;
    recent.push(shade(1));
    recent.push(Rgba{1, 0, 0, 100});
    ASSERT_EQ_INT(2, recent.size());
}

TEST(the_oldest_color_drops_when_full)
{
    RecentColors recent;
    for (int i = 1; i <= RECENT_COLORS_MAX + 3; i++) {
        recent.push(shade(i));
    }
    ASSERT_EQ_INT(RECENT_COLORS_MAX, recent.size());
    ASSERT_EQ_INT(RECENT_COLORS_MAX + 3, recent[0].r);
    ASSERT_EQ_INT(4, recent[RECENT_COLORS_MAX - 1].r);
}

int main()
{
    RUN(create_is_empty);
    RUN(add_returns_sequential_indices);
    RUN(get_out_of_range_is_transparent);
    RUN(set_replaces_color);
    RUN(add_fails_when_full);
    RUN(remove_shifts_later_colors_down);
    RUN(remove_out_of_range_fails);
    RUN(find_matches_exact_colors);
    RUN(rgba_equality_compares_all_four_channels);
    RUN(recent_starts_empty);
    RUN(newest_color_comes_first);
    RUN(pushing_a_known_color_moves_it_to_the_front);
    RUN(alpha_makes_a_different_color);
    RUN(the_oldest_color_drops_when_full);
    return TEST_SUMMARY();
}
