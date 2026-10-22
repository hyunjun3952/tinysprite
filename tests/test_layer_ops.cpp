#include "minitest.h"
#include "tinysprite/composite.h"
#include "tinysprite/layer_ops.h"

#include <string.h>

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

/* Checks the layer names bottom to top; a nullptr name marks the end of the expected list. */
static int names_are(Sprite &s, const char *a, const char *b, const char *c)
{
    const char *want[4] = {a, b, c, nullptr};
    int count = s.layerCount();
    for (int i = 0; i < 4; i++) {
        if (!want[i]) {
            return i == count;
        }
        if (i >= count || strcmp(s.layer(i)->name().c_str(), want[i]) != 0) {
            return 0;
        }
    }
    return 0;
}

TEST(add_is_undoable)
{
    Sprite s(4, 4);
    History h;
    Layer *l = layer_op_add(h, s, "Top");
    ASSERT_EQ_INT(2, s.layerCount());
    ASSERT_TRUE(s.layer(1) == l);
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(1, s.layerCount());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(2, s.layerCount());
    ASSERT_TRUE(s.layer(1) == l);
}

TEST(undone_add_is_freed_when_redo_is_discarded)
{
    Sprite s(4, 4);
    History h;
    layer_op_add(h, s, "Gone");
    h.undo();
    layer_op_add(h, s, "Kept");
    ASSERT_EQ_INT(2, s.layerCount());
    ASSERT_TRUE(names_are(s, "Layer 1", "Kept", nullptr));
}

TEST(undone_add_is_freed_by_history_destroy)
{
    Sprite s(4, 4);
    History h;
    layer_op_add(h, s, "Gone");
    h.undo();
}

TEST(remove_restores_layer_pixels_and_index)
{
    Sprite s(4, 4);
    History h;
    layer_op_add(h, s, "B");
    layer_op_add(h, s, "C");
    Layer *b = s.layer(1);
    h.beginStroke(*b->cel(0));
    h.setPixel(2, 2, RED);
    h.endStroke();

    ASSERT_TRUE(layer_op_remove(h, s, 1));
    ASSERT_EQ_INT(2, s.layerCount());
    ASSERT_TRUE(names_are(s, "Layer 1", "C", nullptr));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(3, s.layerCount());
    ASSERT_TRUE(s.layer(1) == b);
    ASSERT_EQ_INT(255, b->cel(0)->get(2, 2).a);
    /* The stroke that preceded the removal is still undoable on the restored layer. */
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, b->cel(0)->get(2, 2).a);
    ASSERT_TRUE(h.redo());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(2, s.layerCount());
}

TEST(remove_keeps_at_least_one_layer)
{
    Sprite s(4, 4);
    History h;
    ASSERT_TRUE(!layer_op_remove(h, s, 0));
    ASSERT_TRUE(!layer_op_remove(h, s, 5));
    ASSERT_TRUE(!h.canUndo());
}

TEST(removed_layer_is_freed_by_history_destroy)
{
    Sprite s(4, 4);
    History h;
    layer_op_add(h, s, "B");
    layer_op_remove(h, s, 1);
}

TEST(move_is_undoable)
{
    Sprite s(4, 4);
    History h;
    layer_op_add(h, s, "B");
    layer_op_add(h, s, "C");
    ASSERT_TRUE(layer_op_move(h, s, 0, 2));
    ASSERT_TRUE(names_are(s, "B", "C", "Layer 1"));
    h.undo();
    ASSERT_TRUE(names_are(s, "Layer 1", "B", "C"));
    h.redo();
    ASSERT_TRUE(names_are(s, "B", "C", "Layer 1"));
    ASSERT_TRUE(!layer_op_move(h, s, 0, 9));
    ASSERT_TRUE(layer_op_move(h, s, 1, 1));
}

TEST(properties_are_undoable)
{
    Sprite s(4, 4);
    History h;
    Layer *l = s.layer(0);
    ASSERT_TRUE(layer_op_set_visible(h, *l, 0));
    ASSERT_TRUE(layer_op_set_opacity(h, *l, 100));
    ASSERT_TRUE(layer_op_set_name(h, *l, "Body"));
    ASSERT_TRUE(!l->visible());
    ASSERT_EQ_INT(100, l->opacity());
    ASSERT_TRUE(strcmp(l->name().c_str(), "Body") == 0);
    h.undo();
    ASSERT_TRUE(strcmp(l->name().c_str(), "Layer 1") == 0);
    h.undo();
    ASSERT_EQ_INT(255, l->opacity());
    h.undo();
    ASSERT_TRUE(l->visible());
    h.redo();
    h.redo();
    h.redo();
    ASSERT_TRUE(!l->visible());
    ASSERT_EQ_INT(100, l->opacity());
    ASSERT_TRUE(strcmp(l->name().c_str(), "Body") == 0);
}

TEST(lock_is_undoable)
{
    Sprite s(4, 4);
    History h;
    Layer *l = s.layer(0);
    ASSERT_TRUE(layer_op_set_locked(h, *l, true));
    ASSERT_TRUE(l->locked());
    h.undo();
    ASSERT_TRUE(!l->locked());
    h.redo();
    ASSERT_TRUE(l->locked());
    ASSERT_TRUE(layer_op_set_locked(h, *l, true));
    h.undo();
    ASSERT_TRUE(!l->locked());
    ASSERT_TRUE(!h.canUndo());
}

TEST(unchanged_property_records_nothing)
{
    Sprite s(4, 4);
    History h;
    Layer *l = s.layer(0);
    ASSERT_TRUE(layer_op_set_visible(h, *l, 1));
    ASSERT_TRUE(layer_op_set_opacity(h, *l, 255));
    ASSERT_TRUE(layer_op_set_name(h, *l, "Layer 1"));
    ASSERT_TRUE(!h.canUndo());
}

TEST(ops_are_rejected_during_a_stroke)
{
    Sprite s(4, 4);
    History h;
    h.beginStroke(*s.layer(0)->cel(0));
    ASSERT_TRUE(layer_op_add(h, s, "X") == nullptr);
    ASSERT_TRUE(!layer_op_set_visible(h, *s.layer(0), 0));
    ASSERT_TRUE(s.layer(0)->visible());
    ASSERT_EQ_INT(1, s.layerCount());
    h.endStroke();
}

static int red_at(Layer *l, int x, int y)
{
    return l->cel(0)->get(x, y).r;
}

TEST(duplicate_inserts_an_independent_copy_above)
{
    Sprite s(4, 4);
    History h;
    Layer *base = s.layer(0);
    base->setName("Body");
    base->setOpacity(100);
    base->cel(0)->set(2, 1, RED);
    Layer *top = layer_op_add(h, s, "Top");

    Layer *copy = layer_op_duplicate(h, s, 0);
    ASSERT_EQ_INT(3, s.layerCount());
    ASSERT_TRUE(s.layer(1) == copy);
    ASSERT_TRUE(s.layer(2) == top);
    ASSERT_TRUE(strcmp(copy->name().c_str(), "Body copy") == 0);
    ASSERT_EQ_INT(100, copy->opacity());
    ASSERT_EQ_INT(255, red_at(copy, 2, 1));
    copy->cel(0)->set(2, 1, BLUE);
    ASSERT_EQ_INT(255, red_at(base, 2, 1));

    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(2, s.layerCount());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(3, s.layerCount());
    ASSERT_TRUE(s.layer(1) == copy);
}

TEST(duplicate_keeps_visibility_and_truncates_long_names)
{
    Sprite s(2, 2);
    History h;
    char name[LAYER_NAME_MAX + 2];
    memset(name, 'n', LAYER_NAME_MAX);
    name[LAYER_NAME_MAX] = '\0';
    s.layer(0)->setName(name);
    s.layer(0)->setVisible(0);
    Layer *copy = layer_op_duplicate(h, s, 0);
    ASSERT_TRUE(!copy->visible());
    ASSERT_TRUE(strlen(copy->name().c_str()) <= LAYER_NAME_MAX);
    ASSERT_TRUE(strcmp(copy->name().c_str() + strlen(copy->name().c_str()) - 5, " copy") == 0);
}

TEST(duplicate_rejects_bad_index_and_active_stroke)
{
    Sprite s(2, 2);
    History h;
    ASSERT_TRUE(layer_op_duplicate(h, s, 1) == nullptr);
    ASSERT_TRUE(layer_op_duplicate(h, s, -1) == nullptr);
    h.beginStroke(*s.layer(0)->cel(0));
    ASSERT_TRUE(layer_op_duplicate(h, s, 0) == nullptr);
    h.endStroke();
    ASSERT_EQ_INT(1, s.layerCount());
}

/* Bottom layer: red at (0,0) and (1,0). Top layer: blue at (0,0) and (2,0). */
static Sprite merge_fixture(void)
{
    Sprite s(4, 2);
    Layer *top = &s.addLayer("Top");
    s.layer(0)->cel(0)->set(0, 0, RED);
    s.layer(0)->cel(0)->set(1, 0, RED);
    top->cel(0)->set(0, 0, BLUE);
    top->cel(0)->set(2, 0, BLUE);
    return s;
}

TEST(merge_down_flattens_the_layer_into_the_one_below)
{
    Sprite s = merge_fixture();
    History h;
    ASSERT_TRUE(layer_op_merge_down(h, s, 1));
    ASSERT_EQ_INT(1, s.layerCount());
    Cel *cel = s.layer(0)->cel(0);
    ASSERT_EQ_INT(255, cel->get(0, 0).b);
    ASSERT_EQ_INT(0, cel->get(0, 0).r);
    ASSERT_EQ_INT(255, cel->get(1, 0).r);
    ASSERT_EQ_INT(255, cel->get(2, 0).b);
    ASSERT_EQ_INT(0, cel->get(3, 0).a);
    ASSERT_TRUE(strcmp(s.layer(0)->name().c_str(), "Layer 1") == 0);
}

TEST(merge_down_applies_the_upper_layer_opacity)
{
    Sprite s(2, 1);
    Layer *top = &s.addLayer("Top");
    s.layer(0)->cel(0)->set(0, 0, Rgba{0, 0, 0, 255});
    top->cel(0)->set(0, 0, Rgba{255, 255, 255, 255});
    top->setOpacity(128);
    History h;
    ASSERT_TRUE(layer_op_merge_down(h, s, 1));
    Rgba merged = s.layer(0)->cel(0)->get(0, 0);
    ASSERT_EQ_INT(255, merged.a);
    ASSERT_EQ_INT(128, merged.r);
}

TEST(merge_down_undoes_and_redoes_as_one_step)
{
    Sprite s = merge_fixture();
    History h;
    Layer *top = s.layer(1);
    ASSERT_TRUE(layer_op_merge_down(h, s, 1));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(2, s.layerCount());
    ASSERT_TRUE(s.layer(1) == top);
    ASSERT_EQ_INT(255, red_at(s.layer(0), 0, 0));
    ASSERT_EQ_INT(255, top->cel(0)->get(2, 0).b);
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(1, s.layerCount());
    ASSERT_EQ_INT(255, s.layer(0)->cel(0)->get(2, 0).b);
}

TEST(pixel_steps_around_a_merge_stay_valid)
{
    Sprite s(3, 1);
    Layer *top = &s.addLayer("Top");
    History h;
    h.beginStroke(*top->cel(0));
    h.setPixel(1, 0, BLUE);
    h.endStroke();
    ASSERT_TRUE(layer_op_merge_down(h, s, 1));
    h.beginStroke(*s.layer(0)->cel(0));
    h.setPixel(2, 0, RED);
    h.endStroke();

    ASSERT_TRUE(h.undo()); /* the red pixel */
    ASSERT_TRUE(h.undo()); /* the merge */
    ASSERT_EQ_INT(0, s.layer(0)->cel(0)->get(1, 0).a);
    ASSERT_EQ_INT(255, top->cel(0)->get(1, 0).b);
    ASSERT_TRUE(h.undo()); /* the blue pixel, on the restored layer */
    ASSERT_EQ_INT(0, top->cel(0)->get(1, 0).a);
    ASSERT_TRUE(h.redo());
    ASSERT_TRUE(h.redo());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(1, s.layerCount());
    ASSERT_EQ_INT(255, s.layer(0)->cel(0)->get(1, 0).b);
    ASSERT_EQ_INT(255, s.layer(0)->cel(0)->get(2, 0).r);
}

TEST(merge_down_frees_the_merged_layer_with_the_history)
{
    Sprite s = merge_fixture();
    History h;
    ASSERT_TRUE(layer_op_merge_down(h, s, 1));
    /* ASan: the detached layer must be released exactly once when h goes out of scope */
}

TEST(merge_down_is_rejected_when_it_makes_no_sense)
{
    Sprite s = merge_fixture();
    History h;
    ASSERT_TRUE(!layer_op_merge_down(h, s, 0)); /* nothing below */
    ASSERT_TRUE(!layer_op_merge_down(h, s, 2));
    ASSERT_TRUE(!layer_op_merge_down(h, s, -1));
    s.layer(1)->setVisible(0);
    ASSERT_TRUE(!layer_op_merge_down(h, s, 1));
    s.layer(1)->setVisible(1);
    s.layer(0)->setVisible(0);
    ASSERT_TRUE(!layer_op_merge_down(h, s, 1));
    s.layer(0)->setVisible(1);
    h.beginStroke(*s.layer(0)->cel(0));
    ASSERT_TRUE(!layer_op_merge_down(h, s, 1));
    h.endStroke();
    ASSERT_EQ_INT(2, s.layerCount());
}

TEST(can_merge_needs_both_visible_and_the_lower_unlocked)
{
    Sprite s = merge_fixture();
    Layer &lower = *s.layer(0);
    Layer &upper = *s.layer(1);
    ASSERT_TRUE(layer_can_merge(upper, lower));
    upper.setLocked(true); /* the upper layer is only read */
    ASSERT_TRUE(layer_can_merge(upper, lower));
    upper.setLocked(false);
    lower.setLocked(true);
    ASSERT_TRUE(!layer_can_merge(upper, lower));
    lower.setLocked(false);
    upper.setVisible(false);
    ASSERT_TRUE(!layer_can_merge(upper, lower));
    upper.setVisible(true);
    lower.setVisible(false);
    ASSERT_TRUE(!layer_can_merge(upper, lower));
}

TEST(merge_down_is_rejected_onto_a_locked_layer)
{
    Sprite s = merge_fixture();
    History h;
    s.layer(0)->setLocked(true);
    ASSERT_TRUE(!layer_op_merge_down(h, s, 1));
    ASSERT_EQ_INT(2, s.layerCount());
}

TEST(record_name_logs_an_edit_the_layer_already_holds)
{
    Sprite s(4, 4);
    History h;
    Layer *l = s.layer(0);
    l->setName("Typed live");
    ASSERT_TRUE(layer_op_record_name(h, *l, "Layer 1"));
    ASSERT_TRUE(strcmp(l->name().c_str(), "Typed live") == 0);
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(strcmp(l->name().c_str(), "Layer 1") == 0);
    ASSERT_TRUE(h.redo());
    ASSERT_TRUE(strcmp(l->name().c_str(), "Typed live") == 0);
}

TEST(record_opacity_logs_an_edit_the_layer_already_holds)
{
    Sprite s(4, 4);
    History h;
    Layer *l = s.layer(0);
    l->setOpacity(90);
    ASSERT_TRUE(layer_op_record_opacity(h, *l, 255));
    ASSERT_EQ_INT(90, l->opacity());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(255, l->opacity());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(90, l->opacity());
}

TEST(record_records_nothing_when_the_value_is_unchanged)
{
    Sprite s(4, 4);
    History h;
    Layer *l = s.layer(0);
    ASSERT_TRUE(layer_op_record_name(h, *l, "Layer 1"));
    ASSERT_TRUE(layer_op_record_opacity(h, *l, 255));
    ASSERT_TRUE(!h.canUndo());
}

TEST(record_is_rejected_during_a_stroke)
{
    Sprite s(4, 4);
    History h;
    Layer *l = s.layer(0);
    ASSERT_TRUE(h.beginStroke(*l->cel(0)));
    l->setOpacity(90);
    ASSERT_TRUE(!layer_op_record_opacity(h, *l, 255));
    ASSERT_TRUE(!layer_op_record_name(h, *l, "other"));
}

TEST(merge_down_refuses_a_locked_layer_below)
{
    Sprite s = merge_fixture();
    History h;
    s.layer(0)->setLocked(true);
    ASSERT_TRUE(!layer_op_merge_down(h, s, 1));
    ASSERT_EQ_INT(2, s.layerCount());
    s.layer(0)->setLocked(false);
    s.layer(1)->setLocked(true);
    ASSERT_TRUE(layer_op_merge_down(h, s, 1));
}

TEST(duplicate_is_not_locked)
{
    Sprite s(4, 4);
    History h;
    s.layer(0)->setLocked(true);
    Layer *copy = layer_op_duplicate(h, s, 0);
    ASSERT_TRUE(copy && !copy->locked());
}

TEST(blend_mode_change_is_undoable)
{
    Sprite s(4, 4);
    History h;
    Layer *l = s.layer(0);
    ASSERT_TRUE(layer_op_set_blend_mode(h, *l, BlendMode::Multiply));
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Multiply), static_cast<int>(l->blendMode()));
    h.undo();
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Normal), static_cast<int>(l->blendMode()));
    h.redo();
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Multiply), static_cast<int>(l->blendMode()));
}

TEST(blend_mode_unchanged_records_nothing_and_is_refused_while_stroking)
{
    Sprite s(4, 4);
    History h;
    Layer *l = s.layer(0);
    ASSERT_TRUE(layer_op_set_blend_mode(h, *l, BlendMode::Normal));
    ASSERT_TRUE(!h.canUndo());
    h.beginStroke(*l->cel(0));
    ASSERT_TRUE(!layer_op_set_blend_mode(h, *l, BlendMode::Add));
}

TEST(duplicate_keeps_the_blend_mode)
{
    Sprite s(4, 4);
    History h;
    s.layer(0)->setBlendMode(BlendMode::Screen);
    Layer *copy = layer_op_duplicate(h, s, 0);
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Screen), static_cast<int>(copy->blendMode()));
}

TEST(merge_down_bakes_the_upper_blend_mode_into_the_pixels)
{
    Sprite s(1, 1);
    Layer *top = &s.addLayer("Top");
    Rgba back{200, 100, 50, 255};
    Rgba front{128, 255, 10, 255};
    s.layer(0)->cel(0)->set(0, 0, back);
    top->cel(0)->set(0, 0, front);
    top->setBlendMode(BlendMode::Multiply);
    top->setOpacity(200);
    Rgba expected = composite_blend(back, front, 200, BlendMode::Multiply);
    History h;
    ASSERT_TRUE(layer_op_merge_down(h, s, 1));
    ASSERT_TRUE(s.layer(0)->cel(0)->get(0, 0) == expected);
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Normal), static_cast<int>(s.layer(0)->blendMode()));
    h.undo();
    ASSERT_TRUE(s.layer(0)->cel(0)->get(0, 0) == back);
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Multiply), static_cast<int>(s.layer(1)->blendMode()));
}

/* A blend mode merged onto a layer that has visible layers under it would not look like the
 * stack did: the blend saw all of them, the merge only sees the one below. */
TEST(merge_down_with_a_blend_mode_needs_nothing_visible_below_the_lower_layer)
{
    Sprite s(1, 1);
    s.addLayer("Mid");
    s.addLayer("Top").setBlendMode(BlendMode::Multiply);
    History h;
    ASSERT_TRUE(layer_can_merge_down(s, 1)); /* Mid is Normal, Top over the only layer below */
    ASSERT_TRUE(!layer_can_merge_down(s, 2));
    ASSERT_TRUE(!layer_op_merge_down(h, s, 2));
    ASSERT_EQ_INT(3, s.layerCount());
    ASSERT_TRUE(!h.canUndo());
    s.layer(0)->setVisible(false); /* a hidden layer below does not take part in the look */
    ASSERT_TRUE(layer_can_merge_down(s, 2));
    ASSERT_TRUE(layer_op_merge_down(h, s, 2));
}

TEST(merge_down_onto_a_blended_layer_needs_nothing_visible_below_it_either)
{
    Sprite s(1, 1);
    s.addLayer("Mid").setBlendMode(BlendMode::Screen);
    s.addLayer("Top");
    ASSERT_TRUE(!layer_can_merge_down(s, 2)); /* Normal onto Screen, with Base under Screen */
    ASSERT_TRUE(layer_can_merge_down(s, 1));  /* Screen onto the bottom layer */
    s.layer(1)->setBlendMode(BlendMode::Normal);
    ASSERT_TRUE(layer_can_merge_down(s, 2)); /* Normal onto Normal is always exact */
}

TEST(merge_down_with_a_blend_mode_looks_like_the_stack_when_allowed)
{
    Sprite s(2, 1);
    s.addLayer("Top").setBlendMode(BlendMode::Multiply);
    s.layer(0)->cel(0)->set(0, 0, Rgba{200, 100, 50, 255});
    s.layer(1)->cel(0)->set(0, 0, Rgba{128, 255, 10, 255});
    s.layer(1)->cel(0)->set(1, 0, Rgba{128, 255, 10, 255}); /* over nothing: shown unchanged */
    Rgba before0 = composite_pixel(s, 0, 0, 0);
    Rgba before1 = composite_pixel(s, 0, 1, 0);
    History h;
    ASSERT_TRUE(layer_op_merge_down(h, s, 1));
    ASSERT_TRUE(composite_pixel(s, 0, 0, 0) == before0);
    ASSERT_TRUE(composite_pixel(s, 0, 1, 0) == before1);
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
    RUN(lock_is_undoable);
    RUN(merge_down_refuses_a_locked_layer_below);
    RUN(duplicate_is_not_locked);
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
    RUN(can_merge_needs_both_visible_and_the_lower_unlocked);
    RUN(merge_down_is_rejected_onto_a_locked_layer);
    RUN(record_name_logs_an_edit_the_layer_already_holds);
    RUN(record_opacity_logs_an_edit_the_layer_already_holds);
    RUN(record_records_nothing_when_the_value_is_unchanged);
    RUN(record_is_rejected_during_a_stroke);
    RUN(blend_mode_change_is_undoable);
    RUN(blend_mode_unchanged_records_nothing_and_is_refused_while_stroking);
    RUN(duplicate_keeps_the_blend_mode);
    RUN(merge_down_bakes_the_upper_blend_mode_into_the_pixels);
    RUN(merge_down_with_a_blend_mode_needs_nothing_visible_below_the_lower_layer);
    RUN(merge_down_onto_a_blended_layer_needs_nothing_visible_below_it_either);
    RUN(merge_down_with_a_blend_mode_looks_like_the_stack_when_allowed);
    return TEST_SUMMARY();
}
