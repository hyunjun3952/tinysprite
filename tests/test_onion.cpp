#include <cstdlib>
#include <vector>

#include "minitest.hpp"
#include "tinysprite/composite.hpp"
#include "tinysprite/sprite.hpp"

static const Rgba WHITE = {255, 255, 255, 255};
static const Rgba RED = {255, 0, 0, 255};
static const Rgba CLEAR = {0, 0, 0, 0};

/* A 4x1 sprite with `n` frames. */
static Sprite strip(int n)
{
    Sprite s(4, 1);
    for (int i = 1; i < n; i++) {
        s.insertFrame(i, s.makeFrame());
    }
    return s;
}

static std::vector<Rgba> onion(const Sprite &s, int frame, OnionSkin skin)
{
    std::vector<Rgba> out(4, Rgba{1, 2, 3, 4});
    composite_onion(s, frame, skin, Rect{0, 0, 4, 1}, out.data());
    return out;
}

static bool close(int a, int b)
{
    return std::abs(a - b) <= 1;
}

TEST(without_ghosts_it_is_the_plain_frame)
{
    Sprite s = strip(3);
    s.layer(0)->cel(1)->set(2, 0, RED);
    s.layer(0)->cel(0)->set(0, 0, WHITE);
    std::vector<Rgba> out = onion(s, 1, OnionSkin{0, 0, 100});
    ASSERT_TRUE(out[2] == RED);
    ASSERT_TRUE(out[0] == CLEAR);
}

TEST(a_previous_frame_shows_tinted_and_faint)
{
    Sprite s = strip(2);
    s.layer(0)->cel(0)->set(1, 0, WHITE);
    std::vector<Rgba> out = onion(s, 1, OnionSkin{1, 0, 100});
    ASSERT_TRUE(close(out[1].a, 100));
    /* white mixed half and half with the past tint */
    ASSERT_EQ_INT((255 + ONION_PAST_TINT.r + 1) / 2, out[1].r);
    ASSERT_EQ_INT((255 + ONION_PAST_TINT.b + 1) / 2, out[1].b);
    ASSERT_TRUE(out[0] == CLEAR);
}

TEST(a_next_frame_uses_the_other_tint)
{
    Sprite s = strip(2);
    s.layer(0)->cel(1)->set(3, 0, WHITE);
    std::vector<Rgba> out = onion(s, 0, OnionSkin{0, 1, 100});
    ASSERT_TRUE(close(out[3].a, 100));
    ASSERT_EQ_INT((255 + ONION_FUTURE_TINT.b + 1) / 2, out[3].b);
}

TEST(the_current_frame_covers_the_ghosts)
{
    Sprite s = strip(2);
    s.layer(0)->cel(0)->set(1, 0, WHITE);
    s.layer(0)->cel(1)->set(1, 0, RED);
    std::vector<Rgba> out = onion(s, 1, OnionSkin{1, 1, 100});
    ASSERT_TRUE(out[1] == RED);
}

TEST(farther_ghosts_are_fainter)
{
    Sprite s = strip(4);
    s.layer(0)->cel(0)->set(0, 0, WHITE); /* three frames back */
    s.layer(0)->cel(2)->set(1, 0, WHITE); /* one frame back */
    std::vector<Rgba> out = onion(s, 3, OnionSkin{3, 0, 90});
    ASSERT_TRUE(close(out[1].a, 90));
    ASSERT_TRUE(close(out[0].a, 30)); /* 90 * 1 / 3 */
}

TEST(ghosts_stop_at_the_ends_of_the_animation)
{
    Sprite s = strip(2);
    s.layer(0)->cel(1)->set(0, 0, WHITE);
    std::vector<Rgba> out = onion(s, 0, OnionSkin{2, 0, 100}); /* nothing before frame 0 */
    ASSERT_TRUE(out[0] == CLEAR);
    out = onion(s, 1, OnionSkin{0, 2, 100});
    ASSERT_TRUE(out[0] == WHITE); /* the current frame itself */
}

TEST(a_ghost_is_the_flattened_frame_so_hidden_layers_do_not_show)
{
    Sprite s = strip(2);
    s.layer(0)->cel(0)->set(2, 0, WHITE);
    s.layer(0)->setVisible(false);
    std::vector<Rgba> out = onion(s, 1, OnionSkin{1, 0, 100});
    ASSERT_TRUE(out[2] == CLEAR);
}

TEST(past_and_future_ghosts_combine)
{
    Sprite s = strip(3);
    s.layer(0)->cel(0)->set(0, 0, WHITE);
    s.layer(0)->cel(2)->set(0, 0, WHITE);
    std::vector<Rgba> out = onion(s, 1, OnionSkin{1, 1, 100});
    ASSERT_TRUE(out[0].a > 100); /* two translucent layers pile up */
}

TEST(only_the_region_is_written)
{
    Sprite s = strip(2);
    s.layer(0)->cel(0)->set(0, 0, WHITE);
    s.layer(0)->cel(0)->set(3, 0, WHITE);
    std::vector<Rgba> out(4, Rgba{1, 2, 3, 4});
    composite_onion(s, 1, OnionSkin{1, 0, 100}, Rect{3, 0, 1, 1}, out.data());
    ASSERT_TRUE(out[0] == (Rgba{1, 2, 3, 4}));
    ASSERT_TRUE(close(out[3].a, 100));
}

TEST(bad_input_changes_nothing)
{
    Sprite s = strip(2);
    std::vector<Rgba> out(4, Rgba{1, 2, 3, 4});
    composite_onion(s, 5, OnionSkin{1, 1, 100}, Rect{0, 0, 4, 1}, out.data());
    composite_onion(s, 0, OnionSkin{1, 1, 100}, Rect{0, 0, 4, 1}, nullptr);
    ASSERT_TRUE(out[0] == (Rgba{1, 2, 3, 4}));
}

TEST(huge_ghost_counts_are_harmless)
{
    Sprite s = strip(2);
    s.layer(0)->cel(0)->set(0, 0, WHITE);
    std::vector<Rgba> out = onion(s, 1, OnionSkin{1000000, 1000000, 100});
    ASSERT_TRUE(out[0].a > 0);
}

int main()
{
    RUN(without_ghosts_it_is_the_plain_frame);
    RUN(a_previous_frame_shows_tinted_and_faint);
    RUN(a_next_frame_uses_the_other_tint);
    RUN(the_current_frame_covers_the_ghosts);
    RUN(farther_ghosts_are_fainter);
    RUN(ghosts_stop_at_the_ends_of_the_animation);
    RUN(a_ghost_is_the_flattened_frame_so_hidden_layers_do_not_show);
    RUN(past_and_future_ghosts_combine);
    RUN(only_the_region_is_written);
    RUN(bad_input_changes_nothing);
    RUN(huge_ghost_counts_are_harmless);
    return TEST_SUMMARY();
}
