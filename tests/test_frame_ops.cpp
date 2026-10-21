#include "minitest.h"
#include "tinysprite/frame_ops.h"
#include "tinysprite/layer_ops.h"

static const Rgba RED = {255, 0, 0, 255};

TEST(add_is_undoable)
{
    Sprite s(4, 4);
    History h;
    ASSERT_TRUE(frame_op_add(h, s, 1));
    ASSERT_EQ_INT(2, s.frameCount());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(1, s.frameCount());
    ASSERT_EQ_INT(1, s.layer(0)->frameCount());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(2, s.frameCount());
}

TEST(add_rejects_a_bad_index_or_an_open_stroke)
{
    Sprite s(4, 4);
    History h;
    ASSERT_TRUE(!frame_op_add(h, s, 2));
    ASSERT_TRUE(!frame_op_add(h, s, -1));
    h.beginStroke(*s.layer(0)->cel(0));
    ASSERT_TRUE(!frame_op_add(h, s, 1));
    h.cancelStroke();
    ASSERT_EQ_INT(1, s.frameCount());
    ASSERT_TRUE(!h.canUndo());
}

TEST(pixel_steps_survive_adding_and_undoing_frames)
{
    Sprite s(4, 4);
    History h;
    frame_op_add(h, s, 1);
    Cel *second = s.layer(0)->cel(1);
    h.beginStroke(*second);
    h.setPixel(1, 1, RED);
    h.endStroke();
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(h.undo()); /* the frame goes away, and its cel with it */
    ASSERT_EQ_INT(1, s.frameCount());
    ASSERT_TRUE(h.redo());
    ASSERT_TRUE(h.redo());
    ASSERT_TRUE(s.layer(0)->cel(1) == second);
    ASSERT_EQ_INT(255, second->get(1, 1).r);
}

TEST(undone_add_is_freed_when_redo_is_discarded)
{
    Sprite s(4, 4);
    History h;
    frame_op_add(h, s, 1);
    h.undo();
    frame_op_add(h, s, 1);
    ASSERT_EQ_INT(2, s.frameCount());
}

TEST(duplicate_copies_pixels_and_duration_after_the_source)
{
    Sprite s(4, 4);
    History h;
    s.layer(0)->cel(0)->set(2, 2, RED);
    s.setFrameDuration(0, 60);
    ASSERT_TRUE(frame_op_duplicate(h, s, 0));
    ASSERT_EQ_INT(2, s.frameCount());
    ASSERT_EQ_INT(255, s.layer(0)->cel(1)->get(2, 2).r);
    ASSERT_EQ_INT(60, s.frameDuration(1));
    ASSERT_TRUE(s.layer(0)->cel(0) != s.layer(0)->cel(1));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(1, s.frameCount());
    ASSERT_TRUE(!frame_op_duplicate(h, s, 1));
}

TEST(remove_restores_the_frame_with_its_pixels_and_duration)
{
    Sprite s(4, 4);
    History h;
    frame_op_add(h, s, 1);
    Cel *second = s.layer(0)->cel(1);
    h.beginStroke(*second);
    h.setPixel(0, 0, RED);
    h.endStroke();
    s.setFrameDuration(1, 33);

    ASSERT_TRUE(frame_op_remove(h, s, 1));
    ASSERT_EQ_INT(1, s.frameCount());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(2, s.frameCount());
    ASSERT_TRUE(s.layer(0)->cel(1) == second);
    ASSERT_EQ_INT(255, second->get(0, 0).r);
    ASSERT_EQ_INT(33, s.frameDuration(1));
    ASSERT_TRUE(h.undo()); /* the stroke is still undoable on the restored cel */
    ASSERT_EQ_INT(0, second->get(0, 0).a);
}

TEST(the_last_frame_cannot_be_removed)
{
    Sprite s(4, 4);
    History h;
    ASSERT_TRUE(!frame_op_remove(h, s, 0));
    ASSERT_TRUE(!h.canUndo());
}

TEST(move_is_undoable)
{
    Sprite s(4, 4);
    History h;
    frame_op_add(h, s, 1);
    s.layer(0)->cel(0)->set(0, 0, RED);
    ASSERT_TRUE(frame_op_move(h, s, 0, 1));
    ASSERT_EQ_INT(0, s.layer(0)->cel(0)->get(0, 0).a);
    ASSERT_EQ_INT(255, s.layer(0)->cel(1)->get(0, 0).r);
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(255, s.layer(0)->cel(0)->get(0, 0).r);
    ASSERT_TRUE(frame_op_move(h, s, 1, 1));
    ASSERT_TRUE(!frame_op_move(h, s, 0, 2));
}

TEST(set_duration_is_undoable_and_skips_no_ops)
{
    Sprite s(4, 4);
    History h;
    ASSERT_TRUE(frame_op_set_duration(h, s, 0, 250));
    ASSERT_EQ_INT(250, s.frameDuration(0));
    ASSERT_TRUE(frame_op_set_duration(h, s, 0, 250));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(FRAME_DURATION_DEFAULT, s.frameDuration(0));
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(!frame_op_set_duration(h, s, 0, 0));
    ASSERT_TRUE(!frame_op_set_duration(h, s, 3, 50));
}

TEST(layer_ops_and_frame_ops_undo_together)
{
    Sprite s(4, 4);
    History h;
    layer_op_add(h, s, "Top");
    frame_op_add(h, s, 1);
    ASSERT_EQ_INT(2, s.layer(1)->frameCount());
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(1, s.layerCount());
    ASSERT_TRUE(h.redo());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(2, s.layer(1)->frameCount());
}

TEST(a_removed_layer_comes_back_with_the_frames_it_had)
{
    Sprite s(4, 4);
    History h;
    layer_op_add(h, s, "Top");
    frame_op_add(h, s, 1);
    ASSERT_TRUE(layer_op_remove(h, s, 1));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(2, s.layer(1)->frameCount());
}

int main(void)
{
    RUN(add_is_undoable);
    RUN(add_rejects_a_bad_index_or_an_open_stroke);
    RUN(pixel_steps_survive_adding_and_undoing_frames);
    RUN(undone_add_is_freed_when_redo_is_discarded);
    RUN(duplicate_copies_pixels_and_duration_after_the_source);
    RUN(remove_restores_the_frame_with_its_pixels_and_duration);
    RUN(the_last_frame_cannot_be_removed);
    RUN(move_is_undoable);
    RUN(set_duration_is_undoable_and_skips_no_ops);
    RUN(layer_ops_and_frame_ops_undo_together);
    RUN(a_removed_layer_comes_back_with_the_frames_it_had);
    return TEST_SUMMARY();
}
