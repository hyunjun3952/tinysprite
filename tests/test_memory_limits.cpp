#include <cstdint>

#include "minitest.hpp"
#include "tinysprite/frame_ops.hpp"
#include "tinysprite/history.hpp"
#include "tinysprite/layer_ops.hpp"
#include "tinysprite/sprite.hpp"

TEST(the_default_cel_budget_is_what_a_big_sprite_can_still_grow_by)
{
    Sprite s(4096, 4096);           /* 64 MiB a cel */
    ASSERT_TRUE(s.fitsCels(31, 0)); /* 32 cels are exactly 2 GiB */
    ASSERT_TRUE(!s.fitsCels(32, 0));
    ASSERT_TRUE(s.fitsCels(0, 31));
    ASSERT_TRUE(!s.fitsCels(1, 31)); /* 2 layers x 32 frames */
    Sprite small(8, 8);
    ASSERT_TRUE(small.fitsCels(1000, SPRITE_MAX_FRAMES - 1));
    ASSERT_TRUE(!small.fitsCels(0, SPRITE_MAX_FRAMES)); /* the frame limit, not the budget */
}

TEST(layers_and_frames_stop_at_the_cel_budget)
{
    Sprite s(8, 8);
    History h;
    s.setCelBudget(8ULL * 8 * 4 * 4); /* four cels */
    ASSERT_TRUE(layer_op_add(h, s, "b") != nullptr);
    ASSERT_TRUE(frame_op_add(h, s, 1)); /* 2 layers x 2 frames */
    ASSERT_TRUE(layer_op_add(h, s, "c") == nullptr);
    ASSERT_TRUE(layer_op_duplicate(h, s, 0) == nullptr);
    ASSERT_TRUE(!frame_op_add(h, s, 2));
    ASSERT_TRUE(!frame_op_duplicate(h, s, 0));
    ASSERT_EQ_INT(2, s.layerCount());
    ASSERT_EQ_INT(2, s.frameCount());
    h.undo(); /* the frame goes, so there is room for a layer again */
    ASSERT_TRUE(layer_op_add(h, s, "c") != nullptr);
}

TEST(a_removed_layer_makes_room_again)
{
    Sprite s(8, 8);
    History h;
    s.setCelBudget(8ULL * 8 * 4 * 2);
    ASSERT_TRUE(layer_op_add(h, s, "b") != nullptr);
    ASSERT_TRUE(layer_op_add(h, s, "c") == nullptr);
    ASSERT_TRUE(layer_op_remove(h, s, 1));
    ASSERT_TRUE(layer_op_add(h, s, "c") != nullptr);
}

/* A stroke that paints the first n pixels of `cel` with `color`: n deltas when it differs. */
static void stroke(History &h, Cel &cel, int n, Rgba color)
{
    h.beginStroke(cel);
    for (int i = 0; i < n; i++) {
        h.setPixel(i % cel.width(), i / cel.width(), color);
    }
    h.endStroke();
}

static Rgba shade(int i)
{
    return Rgba{static_cast<uint8_t>(i + 1), 0, 0, 255};
}

TEST(the_oldest_steps_are_dropped_beyond_the_history_limit)
{
    History h;
    Cel cel(64, 64);
    h.setLimit(sizeof(uint64_t) * 2 * 4096 * 3); /* a few strokes of 4096 deltas */
    for (int i = 0; i < 10; i++) {
        stroke(h, cel, 64 * 64, shade(i));
    }
    int undone = 0;
    while (h.undo()) {
        undone++;
    }
    ASSERT_TRUE(undone >= 1 && undone < 10);
}

TEST(the_newest_step_is_kept_even_when_it_alone_is_over_the_limit)
{
    History h;
    Cel cel(32, 32);
    h.setLimit(16);
    stroke(h, cel, 32 * 32, shade(0));
    ASSERT_TRUE(h.canUndo());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, cel.get(0, 0).a);
}

TEST(dropping_old_steps_keeps_the_save_point_honest)
{
    History h;
    Cel cel(8, 8);
    h.setLimit(size_t{3} * 12 * 2); /* room for a few deltas */
    stroke(h, cel, 2, shade(0));
    h.markSaved();
    ASSERT_TRUE(h.atSavePoint());
    stroke(h, cel, 2, shade(1));
    ASSERT_TRUE(!h.atSavePoint());
    h.undo();
    ASSERT_TRUE(h.atSavePoint()); /* the save point is still reachable */
    h.redo();
    stroke(h, cel, 6, shade(2)); /* pushes the older steps out */
    stroke(h, cel, 6, shade(3));
    while (h.undo()) {
    }
    ASSERT_TRUE(!h.atSavePoint()); /* the saved state is gone, not mistaken for the oldest one */
}

TEST(an_action_counts_the_bytes_it_declares)
{
    History h;
    h.setLimit(1000);
    for (int i = 0; i < 5; i++) {
        HistoryAction action;
        action.bytes = 400;
        action.undo = [] {};
        action.redo = [] {};
        h.pushAction(std::move(action));
    }
    int undone = 0;
    while (h.undo()) {
        undone++;
    }
    ASSERT_EQ_INT(2, undone);
}

int main()
{
    RUN(the_default_cel_budget_is_what_a_big_sprite_can_still_grow_by);
    RUN(layers_and_frames_stop_at_the_cel_budget);
    RUN(a_removed_layer_makes_room_again);
    RUN(the_oldest_steps_are_dropped_beyond_the_history_limit);
    RUN(the_newest_step_is_kept_even_when_it_alone_is_over_the_limit);
    RUN(dropping_old_steps_keeps_the_save_point_honest);
    RUN(an_action_counts_the_bytes_it_declares);
    return TEST_SUMMARY();
}
