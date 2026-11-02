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

/* Five by four, three frames, with colors that differ per cel, frame and layer: an opaque layer, a
 * half transparent one in Multiply and a hidden one. */
static Sprite varied()
{
    Sprite s(5, 4);
    s.addLayer("multiplied");
    s.addLayer("hidden");
    s.insertFrame(1, s.makeFrame());
    s.insertFrame(2, s.makeFrame());
    s.layer(1)->setBlendMode(BlendMode::Multiply);
    s.layer(1)->setOpacity(180);
    s.layer(2)->setVisible(false);
    for (int l = 0; l < 3; l++) {
        for (int f = 0; f < 3; f++) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 5; x++) {
                    const int mix = 20 * l + 30 * f + 7 * x + 11 * y;
                    s.layer(l)->cel(f)->set(x, y,
                                            Rgba{static_cast<uint8_t>(mix),
                                                 static_cast<uint8_t>(255 - mix),
                                                 static_cast<uint8_t>(x * 40),
                                                 static_cast<uint8_t>(l == 0 ? 255 : 90 + y * 40)});
                }
            }
        }
    }
    return s;
}

/* Makes (x, y) of `frame` see-through on every layer, so the ghosts under it show. */
static void open_hole(Sprite &s, int frame, int x, int y)
{
    for (int l = 0; l < s.layerCount(); l++) {
        s.layer(l)->cel(frame)->set(x, y, CLEAR);
    }
}

/* The ghost of `frame` at (x, y) as composite_onion mixes it with its tint. */
static Rgba ghost(const Sprite &s, int frame, int x, int y, Rgba tint)
{
    const Rgba p = composite_pixel(s, frame, x, y);
    return Rgba{static_cast<uint8_t>((p.r + tint.r + 1) / 2),
                static_cast<uint8_t>((p.g + tint.g + 1) / 2),
                static_cast<uint8_t>((p.b + tint.b + 1) / 2), p.a};
}

/* Frame 1 of varied() over its one ghost on each side, for 3 ghosts wanted before and 1 after at
 * opacity 200: the one after first, as both are as near. */
static Rgba expected_onion(const Sprite &s, int x, int y)
{
    Rgba acc;
    acc = composite_over(acc, ghost(s, 2, x, y, ONION_FUTURE_TINT), 200);
    acc = composite_over(acc, ghost(s, 0, x, y, ONION_PAST_TINT), 200);
    return composite_over(acc, composite_pixel(s, 1, x, y), 255);
}

TEST(a_region_holds_the_ghosts_of_both_sides_far_to_near)
{
    Sprite s = varied();
    open_hole(s, 1, 2, 1);
    OnionSkin skin;
    skin.before = 3; /* frame 1 has one frame before it, so the fade is spread over more */
    skin.after = 1;
    skin.opacity = 200;
    const Rect region{1, 1, 3, 2};
    const Rgba untouched{1, 2, 3, 4};
    std::vector<Rgba> out(20, untouched);
    composite_onion(s, 1, skin, region, out.data());
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 5; x++) {
            const Rgba seen = out[static_cast<size_t>(y) * 5 + static_cast<size_t>(x)];
            ASSERT_TRUE(seen ==
                        (rect_contains(region, x, y) ? expected_onion(s, x, y) : untouched));
        }
    }
}

TEST(a_far_ghost_fades_by_the_count_wanted_on_its_side)
{
    Sprite s = varied();
    open_hole(s, 0, 2, 1);
    open_hole(s, 2, 3, 2);
    OnionSkin skin;
    skin.before = 1;
    skin.after = 3; /* frame 0 has two after it: the far one shows 2 / 3 of the opacity */
    skin.opacity = 200;
    std::vector<Rgba> out(20);
    composite_onion(s, 0, skin, Rect{0, 0, 5, 4}, out.data());
    Rgba expected;
    expected = composite_over(expected, ghost(s, 2, 2, 1, ONION_FUTURE_TINT), 133);
    expected = composite_over(expected, ghost(s, 1, 2, 1, ONION_FUTURE_TINT), 200);
    ASSERT_TRUE(out[1 * 5 + 2] == expected);

    skin.before = 3;
    skin.after = 1;
    composite_onion(s, 2, skin, Rect{0, 0, 5, 4}, out.data());
    expected = Rgba{};
    expected = composite_over(expected, ghost(s, 0, 3, 2, ONION_PAST_TINT), 133);
    expected = composite_over(expected, ghost(s, 1, 3, 2, ONION_PAST_TINT), 200);
    ASSERT_TRUE(out[2 * 5 + 3] == expected);
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
    RUN(a_region_holds_the_ghosts_of_both_sides_far_to_near);
    RUN(a_far_ghost_fades_by_the_count_wanted_on_its_side);
    return TEST_SUMMARY();
}
