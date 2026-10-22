#include <vector>

#include "minitest.h"
#include "tinysprite/composite.h"
#include "tinysprite/sprite.h"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};
static const Rgba CLEAR = {0, 0, 0, 0};

static bool same(Rgba a, Rgba b)
{
    return a == b;
}

static std::vector<Rgba> make_out(const Sprite &s, Rgba fill)
{
    return std::vector<Rgba>(static_cast<size_t>(s.width()) * static_cast<size_t>(s.height()),
                             fill);
}

static Rgba at(const Sprite &s, const std::vector<Rgba> &out, int x, int y)
{
    return out[y * s.width() + x];
}

TEST(blank_sprite_composites_to_transparent)
{
    Sprite s(4, 3);
    std::vector<Rgba> out = make_out(s, RED);
    composite_sprite(s, 0, Rect{0, 0, 4, 3}, out.data());
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            ASSERT_TRUE(same(at(s, out, x, y), CLEAR));
        }
    }
}

TEST(opaque_pixel_is_copied)
{
    Sprite s(4, 3);
    s.layer(0)->cel(0)->set(2, 1, RED);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 4, 3}, out.data());
    ASSERT_TRUE(same(at(s, out, 2, 1), RED));
    ASSERT_TRUE(same(at(s, out, 1, 1), CLEAR));
}

TEST(upper_layer_covers_lower_layer)
{
    Sprite s(2, 2);
    Layer *top = &s.addLayer("top");
    s.layer(0)->cel(0)->set(0, 0, BLUE);
    top->cel(0)->set(0, 0, RED);
    s.layer(0)->cel(0)->set(1, 0, BLUE);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 2, 2}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), RED));
    ASSERT_TRUE(same(at(s, out, 1, 0), BLUE));
}

TEST(hidden_layer_is_skipped)
{
    Sprite s(2, 2);
    Layer *top = &s.addLayer("top");
    s.layer(0)->cel(0)->set(0, 0, BLUE);
    top->cel(0)->set(0, 0, RED);
    top->setVisible(false);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 2, 2}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), BLUE));
}

TEST(layer_opacity_scales_alpha)
{
    Sprite s(2, 2);
    s.layer(0)->cel(0)->set(0, 0, RED);
    s.layer(0)->setOpacity(128);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 2, 2}, out.data());
    Rgba p = at(s, out, 0, 0);
    ASSERT_EQ_INT(255, p.r);
    ASSERT_EQ_INT(0, p.g);
    ASSERT_EQ_INT(0, p.b);
    ASSERT_EQ_INT(128, p.a);
}

TEST(zero_opacity_layer_contributes_nothing)
{
    Sprite s(2, 2);
    s.layer(0)->cel(0)->set(0, 0, RED);
    s.layer(0)->setOpacity(0);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 2, 2}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), CLEAR));
}

TEST(translucent_over_opaque_blends)
{
    Sprite s(2, 2);
    Layer *top = &s.addLayer("top");
    s.layer(0)->cel(0)->set(0, 0, BLUE);
    top->cel(0)->set(0, 0, Rgba{255, 0, 0, 128});
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 2, 2}, out.data());
    Rgba p = at(s, out, 0, 0);
    ASSERT_EQ_INT(255, p.a);
    ASSERT_TRUE(p.r >= 127 && p.r <= 129);
    ASSERT_EQ_INT(0, p.g);
    ASSERT_TRUE(p.b >= 126 && p.b <= 128);
}

TEST(region_only_touches_requested_rect)
{
    Sprite s(4, 4);
    for (int i = 0; i < 4; i++) {
        s.layer(0)->cel(0)->set(i, i, RED);
    }
    std::vector<Rgba> out = make_out(s, BLUE);
    composite_sprite(s, 0, Rect{1, 1, 2, 2}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), BLUE));
    ASSERT_TRUE(same(at(s, out, 1, 1), RED));
    ASSERT_TRUE(same(at(s, out, 2, 2), RED));
    ASSERT_TRUE(same(at(s, out, 2, 1), CLEAR));
    ASSERT_TRUE(same(at(s, out, 3, 3), BLUE));
}

TEST(region_is_clipped_to_the_sprite)
{
    Sprite s(3, 3);
    s.layer(0)->cel(0)->set(2, 2, RED);
    std::vector<Rgba> out = make_out(s, BLUE);
    composite_sprite(s, 0, Rect{-5, -5, 100, 100}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), CLEAR));
    ASSERT_TRUE(same(at(s, out, 2, 2), RED));
    composite_sprite(s, 0, Rect{10, 10, 4, 4}, out.data());
    composite_sprite(s, 0, Rect{0, 0, 0, 0}, out.data());
}

TEST(invalid_frame_leaves_output_untouched)
{
    Sprite s(2, 2);
    std::vector<Rgba> out = make_out(s, BLUE);
    composite_sprite(s, 5, Rect{0, 0, 2, 2}, out.data());
    ASSERT_TRUE(same(at(s, out, 0, 0), BLUE));
}

TEST(composite_pixel_matches_full_composite)
{
    Sprite s(3, 3);
    Layer *top = &s.addLayer("top");
    s.layer(0)->cel(0)->set(1, 1, BLUE);
    top->cel(0)->set(1, 1, RED);
    top->setOpacity(128);
    std::vector<Rgba> out = make_out(s, CLEAR);
    composite_sprite(s, 0, Rect{0, 0, 3, 3}, out.data());
    ASSERT_TRUE(same(composite_pixel(s, 0, 1, 1), at(s, out, 1, 1)));
    ASSERT_TRUE(same(composite_pixel(s, 0, 0, 0), CLEAR));
    ASSERT_TRUE(same(composite_pixel(s, 0, -1, 0), CLEAR));
    ASSERT_TRUE(same(composite_pixel(s, 0, 3, 0), CLEAR));
    ASSERT_TRUE(same(composite_pixel(s, 4, 1, 1), CLEAR));
}

TEST(composite_over_scales_by_opacity)
{
    Rgba black = {0, 0, 0, 255};
    Rgba white = {255, 255, 255, 255};
    Rgba half = composite_over(black, white, 128);
    ASSERT_EQ_INT(255, half.a);
    ASSERT_EQ_INT(128, half.r);
    ASSERT_TRUE(same(composite_over(black, white, 255), white));
    ASSERT_TRUE(same(composite_over(black, white, 0), black));
    ASSERT_TRUE(same(composite_over(CLEAR, white, 255), white));
    ASSERT_EQ_INT(0, composite_over(CLEAR, white, 0).a);
}

int main()
{
    RUN(blank_sprite_composites_to_transparent);
    RUN(opaque_pixel_is_copied);
    RUN(upper_layer_covers_lower_layer);
    RUN(hidden_layer_is_skipped);
    RUN(layer_opacity_scales_alpha);
    RUN(zero_opacity_layer_contributes_nothing);
    RUN(translucent_over_opaque_blends);
    RUN(region_only_touches_requested_rect);
    RUN(region_is_clipped_to_the_sprite);
    RUN(invalid_frame_leaves_output_untouched);
    RUN(composite_pixel_matches_full_composite);
    RUN(composite_over_scales_by_opacity);
    return TEST_SUMMARY();
}
