#include "minitest.h"
#include "tinysprite/eyedropper.h"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

TEST(pick_cel_reads_the_pixel)
{
    Cel *c = cel_create(4, 4);
    cel_set(c, 2, 1, RED);
    Rgba got = {9, 9, 9, 9};
    ASSERT_EQ_INT(0, eyedropper_pick_cel(c, 2, 1, &got));
    ASSERT_EQ_INT(255, got.r);
    ASSERT_EQ_INT(255, got.a);
    ASSERT_EQ_INT(0, eyedropper_pick_cel(c, 0, 0, &got));
    ASSERT_EQ_INT(0, got.a);
    cel_destroy(c);
}

TEST(pick_cel_out_of_bounds_fails_and_keeps_output)
{
    Cel *c = cel_create(4, 4);
    Rgba got = {9, 9, 9, 9};
    ASSERT_EQ_INT(-1, eyedropper_pick_cel(c, 4, 0, &got));
    ASSERT_EQ_INT(-1, eyedropper_pick_cel(c, 0, -1, &got));
    ASSERT_EQ_INT(-1, eyedropper_pick_cel(NULL, 0, 0, &got));
    ASSERT_EQ_INT(-1, eyedropper_pick_cel(c, 0, 0, NULL));
    ASSERT_EQ_INT(9, got.r);
    cel_destroy(c);
}

TEST(pick_sprite_reads_the_flattened_image)
{
    Sprite *s = sprite_create(4, 4);
    Layer *top = sprite_add_layer(s, "top");
    cel_set(layer_cel(sprite_layer(s, 0), 0), 1, 1, BLUE);
    cel_set(layer_cel(top, 0), 1, 1, RED);
    Rgba got;
    ASSERT_EQ_INT(0, eyedropper_pick_sprite(s, 0, 1, 1, &got));
    ASSERT_EQ_INT(255, got.r);
    ASSERT_EQ_INT(0, got.b);
    layer_set_visible(top, 0);
    ASSERT_EQ_INT(0, eyedropper_pick_sprite(s, 0, 1, 1, &got));
    ASSERT_EQ_INT(255, got.b);
    ASSERT_EQ_INT(-1, eyedropper_pick_sprite(s, 0, 9, 9, &got));
    ASSERT_EQ_INT(-1, eyedropper_pick_sprite(s, 3, 1, 1, &got));
    sprite_destroy(s);
}

int main(void)
{
    RUN(pick_cel_reads_the_pixel);
    RUN(pick_cel_out_of_bounds_fails_and_keeps_output);
    RUN(pick_sprite_reads_the_flattened_image);
    return TEST_SUMMARY();
}
