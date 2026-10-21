#include "minitest.h"
#include "tinysprite/layer_ops.h"

#include <string.h>

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

/* Checks the layer names bottom to top; a NULL name marks the end of the expected list. */
static int names_are(Sprite *s, const char *a, const char *b, const char *c)
{
    const char *want[4] = {a, b, c, NULL};
    int count = sprite_layer_count(s);
    for (int i = 0; i < 4; i++) {
        if (!want[i]) {
            return i == count;
        }
        if (i >= count || strcmp(layer_name(sprite_layer(s, i)), want[i]) != 0) {
            return 0;
        }
    }
    return 0;
}

TEST(add_is_undoable)
{
    Sprite *s = sprite_create(4, 4);
    History *h = history_create();
    Layer *l = layer_op_add(h, s, "Top");
    ASSERT_TRUE(l != NULL);
    ASSERT_EQ_INT(2, sprite_layer_count(s));
    ASSERT_TRUE(sprite_layer(s, 1) == l);
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(1, sprite_layer_count(s));
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(2, sprite_layer_count(s));
    ASSERT_TRUE(sprite_layer(s, 1) == l);
    history_destroy(h);
    sprite_destroy(s);
}

TEST(undone_add_is_freed_when_redo_is_discarded)
{
    Sprite *s = sprite_create(4, 4);
    History *h = history_create();
    layer_op_add(h, s, "Gone");
    history_undo(h);
    layer_op_add(h, s, "Kept");
    ASSERT_EQ_INT(2, sprite_layer_count(s));
    ASSERT_TRUE(names_are(s, "Layer 1", "Kept", NULL));
    history_destroy(h);
    sprite_destroy(s);
}

TEST(undone_add_is_freed_by_history_destroy)
{
    Sprite *s = sprite_create(4, 4);
    History *h = history_create();
    layer_op_add(h, s, "Gone");
    history_undo(h);
    history_destroy(h);
    sprite_destroy(s);
}

TEST(remove_restores_layer_pixels_and_index)
{
    Sprite *s = sprite_create(4, 4);
    History *h = history_create();
    layer_op_add(h, s, "B");
    layer_op_add(h, s, "C");
    Layer *b = sprite_layer(s, 1);
    history_begin_stroke(h, layer_cel(b, 0));
    history_set_pixel(h, 2, 2, RED);
    history_end_stroke(h);

    ASSERT_EQ_INT(0, layer_op_remove(h, s, 1));
    ASSERT_EQ_INT(2, sprite_layer_count(s));
    ASSERT_TRUE(names_are(s, "Layer 1", "C", NULL));
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(3, sprite_layer_count(s));
    ASSERT_TRUE(sprite_layer(s, 1) == b);
    ASSERT_EQ_INT(255, cel_get(layer_cel(b, 0), 2, 2).a);
    /* The stroke that preceded the removal is still undoable on the restored layer. */
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(0, cel_get(layer_cel(b, 0), 2, 2).a);
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(2, sprite_layer_count(s));
    history_destroy(h);
    sprite_destroy(s);
}

TEST(remove_keeps_at_least_one_layer)
{
    Sprite *s = sprite_create(4, 4);
    History *h = history_create();
    ASSERT_EQ_INT(-1, layer_op_remove(h, s, 0));
    ASSERT_EQ_INT(-1, layer_op_remove(h, s, 5));
    ASSERT_TRUE(!history_can_undo(h));
    history_destroy(h);
    sprite_destroy(s);
}

TEST(removed_layer_is_freed_by_history_destroy)
{
    Sprite *s = sprite_create(4, 4);
    History *h = history_create();
    layer_op_add(h, s, "B");
    layer_op_remove(h, s, 1);
    history_destroy(h);
    sprite_destroy(s);
}

TEST(move_is_undoable)
{
    Sprite *s = sprite_create(4, 4);
    History *h = history_create();
    layer_op_add(h, s, "B");
    layer_op_add(h, s, "C");
    ASSERT_EQ_INT(0, layer_op_move(h, s, 0, 2));
    ASSERT_TRUE(names_are(s, "B", "C", "Layer 1"));
    history_undo(h);
    ASSERT_TRUE(names_are(s, "Layer 1", "B", "C"));
    history_redo(h);
    ASSERT_TRUE(names_are(s, "B", "C", "Layer 1"));
    ASSERT_EQ_INT(-1, layer_op_move(h, s, 0, 9));
    ASSERT_EQ_INT(0, layer_op_move(h, s, 1, 1));
    history_destroy(h);
    sprite_destroy(s);
}

TEST(properties_are_undoable)
{
    Sprite *s = sprite_create(4, 4);
    History *h = history_create();
    Layer *l = sprite_layer(s, 0);
    ASSERT_EQ_INT(0, layer_op_set_visible(h, l, 0));
    ASSERT_EQ_INT(0, layer_op_set_opacity(h, l, 100));
    ASSERT_EQ_INT(0, layer_op_set_name(h, l, "Body"));
    ASSERT_TRUE(!layer_visible(l));
    ASSERT_EQ_INT(100, layer_opacity(l));
    ASSERT_TRUE(strcmp(layer_name(l), "Body") == 0);
    history_undo(h);
    ASSERT_TRUE(strcmp(layer_name(l), "Layer 1") == 0);
    history_undo(h);
    ASSERT_EQ_INT(255, layer_opacity(l));
    history_undo(h);
    ASSERT_TRUE(layer_visible(l));
    history_redo(h);
    history_redo(h);
    history_redo(h);
    ASSERT_TRUE(!layer_visible(l));
    ASSERT_EQ_INT(100, layer_opacity(l));
    ASSERT_TRUE(strcmp(layer_name(l), "Body") == 0);
    history_destroy(h);
    sprite_destroy(s);
}

TEST(unchanged_property_records_nothing)
{
    Sprite *s = sprite_create(4, 4);
    History *h = history_create();
    Layer *l = sprite_layer(s, 0);
    ASSERT_EQ_INT(0, layer_op_set_visible(h, l, 1));
    ASSERT_EQ_INT(0, layer_op_set_opacity(h, l, 255));
    ASSERT_EQ_INT(0, layer_op_set_name(h, l, "Layer 1"));
    ASSERT_TRUE(!history_can_undo(h));
    history_destroy(h);
    sprite_destroy(s);
}

TEST(ops_are_rejected_during_a_stroke)
{
    Sprite *s = sprite_create(4, 4);
    History *h = history_create();
    history_begin_stroke(h, layer_cel(sprite_layer(s, 0), 0));
    ASSERT_TRUE(layer_op_add(h, s, "X") == NULL);
    ASSERT_EQ_INT(-1, layer_op_set_visible(h, sprite_layer(s, 0), 0));
    ASSERT_TRUE(layer_visible(sprite_layer(s, 0)));
    ASSERT_EQ_INT(1, sprite_layer_count(s));
    history_end_stroke(h);
    history_destroy(h);
    sprite_destroy(s);
}

static int red_at(Layer *l, int x, int y)
{
    return cel_get(layer_cel(l, 0), x, y).r;
}

TEST(duplicate_inserts_an_independent_copy_above)
{
    Sprite *s = sprite_create(4, 4);
    History *h = history_create();
    Layer *base = sprite_layer(s, 0);
    layer_set_name(base, "Body");
    layer_set_opacity(base, 100);
    cel_set(layer_cel(base, 0), 2, 1, RED);
    Layer *top = layer_op_add(h, s, "Top");
    ASSERT_TRUE(top != NULL);

    Layer *copy = layer_op_duplicate(h, s, 0);
    ASSERT_TRUE(copy != NULL);
    ASSERT_EQ_INT(3, sprite_layer_count(s));
    ASSERT_TRUE(sprite_layer(s, 1) == copy);
    ASSERT_TRUE(sprite_layer(s, 2) == top);
    ASSERT_TRUE(strcmp(layer_name(copy), "Body copy") == 0);
    ASSERT_EQ_INT(100, layer_opacity(copy));
    ASSERT_EQ_INT(255, red_at(copy, 2, 1));
    cel_set(layer_cel(copy, 0), 2, 1, BLUE);
    ASSERT_EQ_INT(255, red_at(base, 2, 1));

    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(2, sprite_layer_count(s));
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(3, sprite_layer_count(s));
    ASSERT_TRUE(sprite_layer(s, 1) == copy);
    history_destroy(h);
    sprite_destroy(s);
}

TEST(duplicate_keeps_visibility_and_truncates_long_names)
{
    Sprite *s = sprite_create(2, 2);
    History *h = history_create();
    char name[LAYER_NAME_MAX + 2];
    memset(name, 'n', LAYER_NAME_MAX);
    name[LAYER_NAME_MAX] = '\0';
    layer_set_name(sprite_layer(s, 0), name);
    layer_set_visible(sprite_layer(s, 0), 0);
    Layer *copy = layer_op_duplicate(h, s, 0);
    ASSERT_TRUE(copy != NULL);
    ASSERT_TRUE(!layer_visible(copy));
    ASSERT_TRUE(strlen(layer_name(copy)) <= LAYER_NAME_MAX);
    ASSERT_TRUE(strcmp(layer_name(copy) + strlen(layer_name(copy)) - 5, " copy") == 0);
    history_destroy(h);
    sprite_destroy(s);
}

TEST(duplicate_rejects_bad_index_and_active_stroke)
{
    Sprite *s = sprite_create(2, 2);
    History *h = history_create();
    ASSERT_TRUE(layer_op_duplicate(h, s, 1) == NULL);
    ASSERT_TRUE(layer_op_duplicate(h, s, -1) == NULL);
    ASSERT_TRUE(layer_op_duplicate(NULL, s, 0) == NULL);
    history_begin_stroke(h, layer_cel(sprite_layer(s, 0), 0));
    ASSERT_TRUE(layer_op_duplicate(h, s, 0) == NULL);
    history_end_stroke(h);
    ASSERT_EQ_INT(1, sprite_layer_count(s));
    history_destroy(h);
    sprite_destroy(s);
}

/* Bottom layer: red at (0,0) and (1,0). Top layer: blue at (0,0) and (2,0). */
static Sprite *merge_fixture(void)
{
    Sprite *s = sprite_create(4, 2);
    Layer *top = sprite_add_layer(s, "Top");
    cel_set(layer_cel(sprite_layer(s, 0), 0), 0, 0, RED);
    cel_set(layer_cel(sprite_layer(s, 0), 0), 1, 0, RED);
    cel_set(layer_cel(top, 0), 0, 0, BLUE);
    cel_set(layer_cel(top, 0), 2, 0, BLUE);
    return s;
}

TEST(merge_down_flattens_the_layer_into_the_one_below)
{
    Sprite *s = merge_fixture();
    History *h = history_create();
    ASSERT_EQ_INT(0, layer_op_merge_down(h, s, 1));
    ASSERT_EQ_INT(1, sprite_layer_count(s));
    Cel *cel = layer_cel(sprite_layer(s, 0), 0);
    ASSERT_EQ_INT(255, cel_get(cel, 0, 0).b);
    ASSERT_EQ_INT(0, cel_get(cel, 0, 0).r);
    ASSERT_EQ_INT(255, cel_get(cel, 1, 0).r);
    ASSERT_EQ_INT(255, cel_get(cel, 2, 0).b);
    ASSERT_EQ_INT(0, cel_get(cel, 3, 0).a);
    ASSERT_TRUE(strcmp(layer_name(sprite_layer(s, 0)), "Layer 1") == 0);
    history_destroy(h);
    sprite_destroy(s);
}

TEST(merge_down_applies_the_upper_layer_opacity)
{
    Sprite *s = sprite_create(2, 1);
    Layer *top = sprite_add_layer(s, "Top");
    cel_set(layer_cel(sprite_layer(s, 0), 0), 0, 0, (Rgba){0, 0, 0, 255});
    cel_set(layer_cel(top, 0), 0, 0, (Rgba){255, 255, 255, 255});
    layer_set_opacity(top, 128);
    History *h = history_create();
    ASSERT_EQ_INT(0, layer_op_merge_down(h, s, 1));
    Rgba merged = cel_get(layer_cel(sprite_layer(s, 0), 0), 0, 0);
    ASSERT_EQ_INT(255, merged.a);
    ASSERT_EQ_INT(128, merged.r);
    history_destroy(h);
    sprite_destroy(s);
}

TEST(merge_down_undoes_and_redoes_as_one_step)
{
    Sprite *s = merge_fixture();
    History *h = history_create();
    Layer *top = sprite_layer(s, 1);
    ASSERT_EQ_INT(0, layer_op_merge_down(h, s, 1));
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(2, sprite_layer_count(s));
    ASSERT_TRUE(sprite_layer(s, 1) == top);
    ASSERT_EQ_INT(255, red_at(sprite_layer(s, 0), 0, 0));
    ASSERT_EQ_INT(255, cel_get(layer_cel(top, 0), 2, 0).b);
    ASSERT_TRUE(!history_can_undo(h));
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(1, sprite_layer_count(s));
    ASSERT_EQ_INT(255, cel_get(layer_cel(sprite_layer(s, 0), 0), 2, 0).b);
    history_destroy(h);
    sprite_destroy(s);
}

TEST(pixel_steps_around_a_merge_stay_valid)
{
    Sprite *s = sprite_create(3, 1);
    Layer *top = sprite_add_layer(s, "Top");
    History *h = history_create();
    history_begin_stroke(h, layer_cel(top, 0));
    history_set_pixel(h, 1, 0, BLUE);
    history_end_stroke(h);
    ASSERT_EQ_INT(0, layer_op_merge_down(h, s, 1));
    history_begin_stroke(h, layer_cel(sprite_layer(s, 0), 0));
    history_set_pixel(h, 2, 0, RED);
    history_end_stroke(h);

    ASSERT_EQ_INT(0, history_undo(h)); /* the red pixel */
    ASSERT_EQ_INT(0, history_undo(h)); /* the merge */
    ASSERT_EQ_INT(0, cel_get(layer_cel(sprite_layer(s, 0), 0), 1, 0).a);
    ASSERT_EQ_INT(255, cel_get(layer_cel(top, 0), 1, 0).b);
    ASSERT_EQ_INT(0, history_undo(h)); /* the blue pixel, on the restored layer */
    ASSERT_EQ_INT(0, cel_get(layer_cel(top, 0), 1, 0).a);
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(1, sprite_layer_count(s));
    ASSERT_EQ_INT(255, cel_get(layer_cel(sprite_layer(s, 0), 0), 1, 0).b);
    ASSERT_EQ_INT(255, cel_get(layer_cel(sprite_layer(s, 0), 0), 2, 0).r);
    history_destroy(h);
    sprite_destroy(s);
}

TEST(merge_down_frees_the_merged_layer_with_the_history)
{
    Sprite *s = merge_fixture();
    History *h = history_create();
    ASSERT_EQ_INT(0, layer_op_merge_down(h, s, 1));
    history_destroy(h); /* ASan: the detached layer must be released exactly once */
    sprite_destroy(s);
}

TEST(merge_down_is_rejected_when_it_makes_no_sense)
{
    Sprite *s = merge_fixture();
    History *h = history_create();
    ASSERT_EQ_INT(-1, layer_op_merge_down(h, s, 0)); /* nothing below */
    ASSERT_EQ_INT(-1, layer_op_merge_down(h, s, 2));
    ASSERT_EQ_INT(-1, layer_op_merge_down(h, s, -1));
    ASSERT_EQ_INT(-1, layer_op_merge_down(NULL, s, 1));
    layer_set_visible(sprite_layer(s, 1), 0);
    ASSERT_EQ_INT(-1, layer_op_merge_down(h, s, 1));
    layer_set_visible(sprite_layer(s, 1), 1);
    layer_set_visible(sprite_layer(s, 0), 0);
    ASSERT_EQ_INT(-1, layer_op_merge_down(h, s, 1));
    layer_set_visible(sprite_layer(s, 0), 1);
    history_begin_stroke(h, layer_cel(sprite_layer(s, 0), 0));
    ASSERT_EQ_INT(-1, layer_op_merge_down(h, s, 1));
    history_end_stroke(h);
    ASSERT_EQ_INT(2, sprite_layer_count(s));
    history_destroy(h);
    sprite_destroy(s);
}

int main(void)
{
    RUN(add_is_undoable);
    RUN(undone_add_is_freed_when_redo_is_discarded);
    RUN(undone_add_is_freed_by_history_destroy);
    RUN(remove_restores_layer_pixels_and_index);
    RUN(remove_keeps_at_least_one_layer);
    RUN(removed_layer_is_freed_by_history_destroy);
    RUN(move_is_undoable);
    RUN(properties_are_undoable);
    RUN(unchanged_property_records_nothing);
    RUN(ops_are_rejected_during_a_stroke);
    RUN(duplicate_inserts_an_independent_copy_above);
    RUN(duplicate_keeps_visibility_and_truncates_long_names);
    RUN(duplicate_rejects_bad_index_and_active_stroke);
    RUN(merge_down_flattens_the_layer_into_the_one_below);
    RUN(merge_down_applies_the_upper_layer_opacity);
    RUN(merge_down_undoes_and_redoes_as_one_step);
    RUN(pixel_steps_around_a_merge_stay_valid);
    RUN(merge_down_frees_the_merged_layer_with_the_history);
    RUN(merge_down_is_rejected_when_it_makes_no_sense);
    return TEST_SUMMARY();
}
