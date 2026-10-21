#include "minitest.h"
#include "tinysprite/sprite.h"

#include <string.h>

TEST(create_has_one_frame_and_one_layer)
{
    Sprite *s = sprite_create(16, 8);
    ASSERT_TRUE(s != NULL);
    ASSERT_EQ_INT(16, sprite_width(s));
    ASSERT_EQ_INT(8, sprite_height(s));
    ASSERT_EQ_INT(1, sprite_frame_count(s));
    ASSERT_EQ_INT(1, sprite_layer_count(s));
    ASSERT_TRUE(strcmp(layer_name(sprite_layer(s, 0)), "Layer 1") == 0);
    ASSERT_EQ_INT(0, palette_count(sprite_palette(s)));
    sprite_destroy(s);
}

TEST(create_rejects_invalid_size)
{
    ASSERT_TRUE(sprite_create(0, 8) == NULL);
    ASSERT_TRUE(sprite_create(8, -1) == NULL);
    ASSERT_TRUE(sprite_create(CEL_MAX_DIMENSION + 1, 8) == NULL);
}

TEST(destroy_null_is_safe)
{
    sprite_destroy(NULL);
}

TEST(first_layer_cel_matches_sprite_size)
{
    Sprite *s = sprite_create(16, 8);
    Cel *c = layer_cel(sprite_layer(s, 0), 0);
    ASSERT_TRUE(c != NULL);
    ASSERT_EQ_INT(16, cel_width(c));
    ASSERT_EQ_INT(8, cel_height(c));
    sprite_destroy(s);
}

TEST(add_layer_appends_on_top)
{
    Sprite *s = sprite_create(4, 4);
    Layer *top = sprite_add_layer(s, "Top");
    ASSERT_TRUE(top != NULL);
    ASSERT_EQ_INT(2, sprite_layer_count(s));
    ASSERT_TRUE(sprite_layer(s, 1) == top);
    ASSERT_TRUE(strcmp(layer_name(top), "Top") == 0);
    ASSERT_EQ_INT(4, cel_width(layer_cel(top, 0)));
    sprite_destroy(s);
}

TEST(add_many_layers_keeps_pointers_valid)
{
    Sprite *s = sprite_create(4, 4);
    Layer *first = sprite_layer(s, 0);
    for (int i = 0; i < 100; i++) {
        ASSERT_TRUE(sprite_add_layer(s, "x") != NULL);
    }
    ASSERT_EQ_INT(101, sprite_layer_count(s));
    ASSERT_TRUE(sprite_layer(s, 0) == first);
    sprite_destroy(s);
}

TEST(layer_out_of_range_is_null)
{
    Sprite *s = sprite_create(4, 4);
    ASSERT_TRUE(sprite_layer(s, -1) == NULL);
    ASSERT_TRUE(sprite_layer(s, 1) == NULL);
    sprite_destroy(s);
}

TEST(remove_layer_shifts_upper_layers_down)
{
    Sprite *s = sprite_create(4, 4);
    sprite_add_layer(s, "B");
    sprite_add_layer(s, "C");
    ASSERT_EQ_INT(0, sprite_remove_layer(s, 1));
    ASSERT_EQ_INT(2, sprite_layer_count(s));
    ASSERT_TRUE(strcmp(layer_name(sprite_layer(s, 0)), "Layer 1") == 0);
    ASSERT_TRUE(strcmp(layer_name(sprite_layer(s, 1)), "C") == 0);
    ASSERT_TRUE(sprite_layer(s, 2) == NULL);
    sprite_destroy(s);
}

TEST(remove_layer_keeps_at_least_one_layer)
{
    Sprite *s = sprite_create(4, 4);
    ASSERT_EQ_INT(-1, sprite_remove_layer(s, 0));
    ASSERT_EQ_INT(1, sprite_layer_count(s));
    sprite_add_layer(s, "B");
    ASSERT_EQ_INT(-1, sprite_remove_layer(s, 2));
    ASSERT_EQ_INT(-1, sprite_remove_layer(s, -1));
    ASSERT_EQ_INT(2, sprite_layer_count(s));
    sprite_destroy(s);
}

TEST(move_layer_up_and_down)
{
    Sprite *s = sprite_create(4, 4);
    sprite_add_layer(s, "B");
    sprite_add_layer(s, "C");
    ASSERT_EQ_INT(0, sprite_move_layer(s, 0, 2)); /* Layer 1 to the top */
    ASSERT_TRUE(strcmp(layer_name(sprite_layer(s, 0)), "B") == 0);
    ASSERT_TRUE(strcmp(layer_name(sprite_layer(s, 1)), "C") == 0);
    ASSERT_TRUE(strcmp(layer_name(sprite_layer(s, 2)), "Layer 1") == 0);
    ASSERT_EQ_INT(0, sprite_move_layer(s, 2, 1));
    ASSERT_TRUE(strcmp(layer_name(sprite_layer(s, 1)), "Layer 1") == 0);
    ASSERT_TRUE(strcmp(layer_name(sprite_layer(s, 2)), "C") == 0);
    sprite_destroy(s);
}

TEST(move_layer_same_index_is_noop_and_bad_index_fails)
{
    Sprite *s = sprite_create(4, 4);
    sprite_add_layer(s, "B");
    ASSERT_EQ_INT(0, sprite_move_layer(s, 1, 1));
    ASSERT_TRUE(strcmp(layer_name(sprite_layer(s, 1)), "B") == 0);
    ASSERT_EQ_INT(-1, sprite_move_layer(s, 0, 2));
    ASSERT_EQ_INT(-1, sprite_move_layer(s, -1, 0));
    sprite_destroy(s);
}

TEST(detach_and_insert_round_trip)
{
    Sprite *s = sprite_create(4, 4);
    Layer *a = sprite_layer(s, 0);
    Layer *b = sprite_add_layer(s, "B");
    Layer *c = sprite_add_layer(s, "C");
    ASSERT_TRUE(sprite_detach_layer(s, 1) == b);
    ASSERT_EQ_INT(2, sprite_layer_count(s));
    ASSERT_TRUE(sprite_layer(s, 0) == a && sprite_layer(s, 1) == c);
    ASSERT_TRUE(cel_width(layer_cel(b, 0)) == 4); /* still alive */
    ASSERT_EQ_INT(0, sprite_insert_layer(s, 1, b));
    ASSERT_TRUE(sprite_layer(s, 1) == b && sprite_layer(s, 2) == c);
    sprite_destroy(s);
}

TEST(detach_and_insert_reject_bad_arguments)
{
    Sprite *s = sprite_create(4, 4);
    ASSERT_TRUE(sprite_detach_layer(s, 0) == NULL); /* last layer */
    sprite_add_layer(s, "B");
    ASSERT_TRUE(sprite_detach_layer(s, 5) == NULL);
    ASSERT_TRUE(sprite_detach_layer(s, -1) == NULL);
    Layer *spare = layer_create("X", 4, 4, 1);
    ASSERT_EQ_INT(-1, sprite_insert_layer(s, 3, spare));
    ASSERT_EQ_INT(-1, sprite_insert_layer(s, -1, spare));
    ASSERT_EQ_INT(-1, sprite_insert_layer(s, 0, NULL));
    ASSERT_EQ_INT(0, sprite_insert_layer(s, 2, spare)); /* append */
    ASSERT_TRUE(sprite_layer(s, 2) == spare);
    sprite_destroy(s);
}

int main(void)
{
    RUN(create_has_one_frame_and_one_layer);
    RUN(create_rejects_invalid_size);
    RUN(destroy_null_is_safe);
    RUN(first_layer_cel_matches_sprite_size);
    RUN(add_layer_appends_on_top);
    RUN(add_many_layers_keeps_pointers_valid);
    RUN(layer_out_of_range_is_null);
    RUN(remove_layer_shifts_upper_layers_down);
    RUN(remove_layer_keeps_at_least_one_layer);
    RUN(move_layer_up_and_down);
    RUN(move_layer_same_index_is_noop_and_bad_index_fails);
    RUN(detach_and_insert_round_trip);
    RUN(detach_and_insert_reject_bad_arguments);
    return TEST_SUMMARY();
}
