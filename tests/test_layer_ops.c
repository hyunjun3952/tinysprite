#include "minitest.h"
#include "tinysprite/layer_ops.h"

#include <string.h>

static const Rgba RED = {255, 0, 0, 255};

static int names_are(Sprite *s, const char *a, const char *b, const char *c)
{
    const char *want[3] = {a, b, c};
    for (int i = 0; i < sprite_layer_count(s); i++) {
        if (!want[i] || strcmp(layer_name(sprite_layer(s, i)), want[i]) != 0) {
            return 0;
        }
    }
    return 1;
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
    return TEST_SUMMARY();
}
