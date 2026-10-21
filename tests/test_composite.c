#include <stdlib.h>

#include "minitest.h"
#include "tinysprite/composite.h"
#include "tinysprite/sprite.h"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};
static const Rgba CLEAR = {0, 0, 0, 0};

static int same(Rgba a, Rgba b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static Rgba *make_out(const Sprite *s, Rgba fill)
{
    size_t n = (size_t)sprite_width(s) * (size_t)sprite_height(s);
    Rgba *out = malloc(n * sizeof(*out));
    for (size_t i = 0; i < n; i++) {
        out[i] = fill;
    }
    return out;
}

static Rgba at(const Sprite *s, const Rgba *out, int x, int y)
{
    return out[y * sprite_width(s) + x];
}

TEST(blank_sprite_composites_to_transparent)
{
    Sprite *s = sprite_create(4, 3);
    Rgba *out = make_out(s, RED);
    composite_sprite(s, 0, (CompositeRect){0, 0, 4, 3}, out);
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            ASSERT_TRUE(same(at(s, out, x, y), CLEAR));
        }
    }
    free(out);
    sprite_destroy(s);
}

TEST(opaque_pixel_is_copied)
{
    Sprite *s = sprite_create(4, 3);
    cel_set(layer_cel(sprite_layer(s, 0), 0), 2, 1, RED);
    Rgba *out = make_out(s, CLEAR);
    composite_sprite(s, 0, (CompositeRect){0, 0, 4, 3}, out);
    ASSERT_TRUE(same(at(s, out, 2, 1), RED));
    ASSERT_TRUE(same(at(s, out, 1, 1), CLEAR));
    free(out);
    sprite_destroy(s);
}

TEST(upper_layer_covers_lower_layer)
{
    Sprite *s = sprite_create(2, 2);
    Layer *top = sprite_add_layer(s, "top");
    cel_set(layer_cel(sprite_layer(s, 0), 0), 0, 0, BLUE);
    cel_set(layer_cel(top, 0), 0, 0, RED);
    cel_set(layer_cel(sprite_layer(s, 0), 0), 1, 0, BLUE);
    Rgba *out = make_out(s, CLEAR);
    composite_sprite(s, 0, (CompositeRect){0, 0, 2, 2}, out);
    ASSERT_TRUE(same(at(s, out, 0, 0), RED));
    ASSERT_TRUE(same(at(s, out, 1, 0), BLUE));
    free(out);
    sprite_destroy(s);
}

TEST(hidden_layer_is_skipped)
{
    Sprite *s = sprite_create(2, 2);
    Layer *top = sprite_add_layer(s, "top");
    cel_set(layer_cel(sprite_layer(s, 0), 0), 0, 0, BLUE);
    cel_set(layer_cel(top, 0), 0, 0, RED);
    layer_set_visible(top, 0);
    Rgba *out = make_out(s, CLEAR);
    composite_sprite(s, 0, (CompositeRect){0, 0, 2, 2}, out);
    ASSERT_TRUE(same(at(s, out, 0, 0), BLUE));
    free(out);
    sprite_destroy(s);
}

TEST(layer_opacity_scales_alpha)
{
    Sprite *s = sprite_create(2, 2);
    cel_set(layer_cel(sprite_layer(s, 0), 0), 0, 0, RED);
    layer_set_opacity(sprite_layer(s, 0), 128);
    Rgba *out = make_out(s, CLEAR);
    composite_sprite(s, 0, (CompositeRect){0, 0, 2, 2}, out);
    Rgba p = at(s, out, 0, 0);
    ASSERT_EQ_INT(255, p.r);
    ASSERT_EQ_INT(0, p.g);
    ASSERT_EQ_INT(0, p.b);
    ASSERT_EQ_INT(128, p.a);
    free(out);
    sprite_destroy(s);
}

TEST(zero_opacity_layer_contributes_nothing)
{
    Sprite *s = sprite_create(2, 2);
    cel_set(layer_cel(sprite_layer(s, 0), 0), 0, 0, RED);
    layer_set_opacity(sprite_layer(s, 0), 0);
    Rgba *out = make_out(s, CLEAR);
    composite_sprite(s, 0, (CompositeRect){0, 0, 2, 2}, out);
    ASSERT_TRUE(same(at(s, out, 0, 0), CLEAR));
    free(out);
    sprite_destroy(s);
}

TEST(translucent_over_opaque_blends)
{
    Sprite *s = sprite_create(2, 2);
    Layer *top = sprite_add_layer(s, "top");
    cel_set(layer_cel(sprite_layer(s, 0), 0), 0, 0, BLUE);
    cel_set(layer_cel(top, 0), 0, 0, (Rgba){255, 0, 0, 128});
    Rgba *out = make_out(s, CLEAR);
    composite_sprite(s, 0, (CompositeRect){0, 0, 2, 2}, out);
    Rgba p = at(s, out, 0, 0);
    ASSERT_EQ_INT(255, p.a);
    ASSERT_TRUE(p.r >= 127 && p.r <= 129);
    ASSERT_EQ_INT(0, p.g);
    ASSERT_TRUE(p.b >= 126 && p.b <= 128);
    free(out);
    sprite_destroy(s);
}

TEST(region_only_touches_requested_rect)
{
    Sprite *s = sprite_create(4, 4);
    for (int i = 0; i < 4; i++) {
        cel_set(layer_cel(sprite_layer(s, 0), 0), i, i, RED);
    }
    Rgba *out = make_out(s, BLUE);
    composite_sprite(s, 0, (CompositeRect){1, 1, 2, 2}, out);
    ASSERT_TRUE(same(at(s, out, 0, 0), BLUE));
    ASSERT_TRUE(same(at(s, out, 1, 1), RED));
    ASSERT_TRUE(same(at(s, out, 2, 2), RED));
    ASSERT_TRUE(same(at(s, out, 2, 1), CLEAR));
    ASSERT_TRUE(same(at(s, out, 3, 3), BLUE));
    free(out);
    sprite_destroy(s);
}

TEST(region_is_clipped_to_the_sprite)
{
    Sprite *s = sprite_create(3, 3);
    cel_set(layer_cel(sprite_layer(s, 0), 0), 2, 2, RED);
    Rgba *out = make_out(s, BLUE);
    composite_sprite(s, 0, (CompositeRect){-5, -5, 100, 100}, out);
    ASSERT_TRUE(same(at(s, out, 0, 0), CLEAR));
    ASSERT_TRUE(same(at(s, out, 2, 2), RED));
    composite_sprite(s, 0, (CompositeRect){10, 10, 4, 4}, out);
    composite_sprite(s, 0, (CompositeRect){0, 0, 0, 0}, out);
    free(out);
    sprite_destroy(s);
}

TEST(invalid_frame_leaves_output_untouched)
{
    Sprite *s = sprite_create(2, 2);
    Rgba *out = make_out(s, BLUE);
    composite_sprite(s, 5, (CompositeRect){0, 0, 2, 2}, out);
    ASSERT_TRUE(same(at(s, out, 0, 0), BLUE));
    composite_sprite(NULL, 0, (CompositeRect){0, 0, 2, 2}, out);
    free(out);
    sprite_destroy(s);
}

int main(void)
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
    return TEST_SUMMARY();
}
