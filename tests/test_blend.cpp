#include <cstring>
#include <vector>

#include "minitest.h"
#include "tinysprite/composite.h"
#include "tinysprite/sprite.h"

static const Rgba CLEAR = {0, 0, 0, 0};
static const Rgba BACK = {200, 100, 50, 255};
static const Rgba FRONT = {128, 255, 10, 255};

static void assert_rgba(Rgba got, int r, int g, int b, int a, int line)
{
    if (got.r != r || got.g != g || got.b != b || got.a != a) {
        std::printf("  line %d: got (%d,%d,%d,%d), want (%d,%d,%d,%d)\n", line, got.r, got.g, got.b,
                    got.a, r, g, b, a);
        ASSERT_TRUE(0);
    }
}
#define ASSERT_RGBA(got, r, g, b, a) assert_rgba(got, r, g, b, a, __LINE__)

TEST(layer_defaults_to_normal)
{
    Layer l("l", 2, 2, 1);
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Normal), static_cast<int>(l.blendMode()));
    l.setBlendMode(BlendMode::Screen);
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Screen), static_cast<int>(l.blendMode()));
}

TEST(blend_mode_names)
{
    ASSERT_EQ_INT(8, BLEND_MODE_COUNT);
    ASSERT_TRUE(std::strcmp(blend_mode_name(BlendMode::Normal), "Normal") == 0);
    ASSERT_TRUE(std::strcmp(blend_mode_name(BlendMode::Difference), "Difference") == 0);
    for (int i = 0; i < BLEND_MODE_COUNT; i++) {
        ASSERT_TRUE(std::strlen(blend_mode_name(static_cast<BlendMode>(i))) > 0);
    }
}

TEST(normal_equals_composite_over)
{
    Rgba backs[] = {CLEAR, BACK, {10, 20, 30, 77}};
    Rgba fronts[] = {CLEAR, FRONT, {5, 6, 7, 99}};
    uint8_t opacities[] = {0, 128, 255};
    for (Rgba b : backs) {
        for (Rgba f : fronts) {
            for (uint8_t o : opacities) {
                ASSERT_TRUE(composite_blend(b, f, o, BlendMode::Normal) == composite_over(b, f, o));
            }
        }
    }
}

TEST(opaque_on_opaque_per_mode)
{
    ASSERT_RGBA(composite_blend(BACK, FRONT, 255, BlendMode::Multiply), 100, 100, 2, 255);
    ASSERT_RGBA(composite_blend(BACK, FRONT, 255, BlendMode::Screen), 228, 255, 58, 255);
    ASSERT_RGBA(composite_blend(BACK, FRONT, 255, BlendMode::Overlay), 200, 200, 4, 255);
    ASSERT_RGBA(composite_blend(BACK, FRONT, 255, BlendMode::Darken), 128, 100, 10, 255);
    ASSERT_RGBA(composite_blend(BACK, FRONT, 255, BlendMode::Lighten), 200, 255, 50, 255);
    ASSERT_RGBA(composite_blend(BACK, FRONT, 255, BlendMode::Add), 255, 255, 60, 255);
    ASSERT_RGBA(composite_blend(BACK, FRONT, 255, BlendMode::Difference), 72, 155, 40, 255);
}

TEST(half_opacity_mixes_the_blended_color)
{
    /* Multiply gives (100, 100, 2); half of it over (200, 100, 50) */
    Rgba out = composite_blend(BACK, FRONT, 128, BlendMode::Multiply);
    ASSERT_RGBA(out, 150, 100, 26, 255);
}

TEST(transparent_backdrop_shows_the_source)
{
    for (int m = 0; m < BLEND_MODE_COUNT; m++) {
        auto mode = static_cast<BlendMode>(m);
        ASSERT_TRUE(composite_blend(CLEAR, FRONT, 255, mode) == FRONT);
        ASSERT_RGBA(composite_blend(CLEAR, FRONT, 128, mode), 128, 255, 10, 128);
    }
}

TEST(transparent_source_leaves_the_backdrop)
{
    for (int m = 0; m < BLEND_MODE_COUNT; m++) {
        auto mode = static_cast<BlendMode>(m);
        ASSERT_TRUE(composite_blend(BACK, CLEAR, 255, mode) == BACK);
        ASSERT_TRUE(composite_blend(BACK, FRONT, 0, mode) == BACK);
    }
}

TEST(translucent_backdrop_mixes_source_and_blend)
{
    /* backdrop alpha 128: Cs' = (1 - ab) * Cs + ab * B */
    Rgba back = {200, 100, 50, 128};
    Rgba out = composite_blend(back, FRONT, 255, BlendMode::Multiply);
    ASSERT_EQ_INT(114, out.r);
    ASSERT_EQ_INT(177, out.g);
    ASSERT_EQ_INT(255, out.a);
}

TEST(sprite_composite_uses_the_layer_mode)
{
    Sprite s(2, 1);
    Layer &top = s.addLayer("shade");
    s.layer(0)->cel(0)->set(0, 0, BACK);
    s.layer(0)->cel(0)->set(1, 0, BACK);
    top.cel(0)->set(0, 0, FRONT);
    top.setBlendMode(BlendMode::Multiply);

    std::vector<Rgba> out(2);
    composite_sprite(s, 0, Rect{0, 0, 2, 1}, out.data());
    ASSERT_RGBA(out[0], 100, 100, 2, 255);
    ASSERT_TRUE(out[1] == BACK);
    ASSERT_TRUE(composite_pixel(s, 0, 0, 0) == out[0]);
    ASSERT_TRUE(composite_pixel(s, 0, 1, 0) == out[1]);
}

TEST(mode_on_the_bottom_layer_keeps_the_pixels)
{
    Sprite s(1, 1);
    s.layer(0)->cel(0)->set(0, 0, FRONT);
    s.layer(0)->setBlendMode(BlendMode::Multiply);
    ASSERT_TRUE(composite_pixel(s, 0, 0, 0) == FRONT);
}

TEST(onion_ghosts_stay_plain_over)
{
    Sprite s(1, 1);
    s.insertFrame(1, s.makeFrame());
    s.layer(0)->setBlendMode(BlendMode::Add);
    s.layer(0)->cel(1)->set(0, 0, FRONT);
    OnionSkin skin;
    skin.before = 0;
    skin.after = 1;
    std::vector<Rgba> out(1);
    composite_onion(s, 0, skin, Rect{0, 0, 1, 1}, out.data());
    /* frame 1 is a ghost under an empty frame 0, drawn plain over a transparent backdrop */
    ASSERT_TRUE(out[0].a > 0);
}

int main()
{
    RUN(layer_defaults_to_normal);
    RUN(blend_mode_names);
    RUN(normal_equals_composite_over);
    RUN(opaque_on_opaque_per_mode);
    RUN(half_opacity_mixes_the_blended_color);
    RUN(transparent_backdrop_shows_the_source);
    RUN(transparent_source_leaves_the_backdrop);
    RUN(translucent_backdrop_mixes_source_and_blend);
    RUN(sprite_composite_uses_the_layer_mode);
    RUN(mode_on_the_bottom_layer_keeps_the_pixels);
    RUN(onion_ghosts_stay_plain_over);
    return TEST_SUMMARY();
}
