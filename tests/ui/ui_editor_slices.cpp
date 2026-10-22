/* Headless tests for the slices of the UI editor: adding, editing and removing them (one undo step
 * each), the selected slice, and the slice tool's drags. Built only when the UI is built; runs
 * under SDL_VIDEODRIVER=dummy. */

#include <optional>
#include <string>

#include "editor.hpp"
#include "minitest.hpp"
#include "ui_editor_support.hpp"

static int slice_count(Editor *e)
{
    return static_cast<int>(e->sprite().slices().size());
}

TEST(adding_a_slice_names_it_selects_it_and_is_one_undo_step)
{
    Editor *e = make_editor();
    ASSERT_TRUE(e->addSlice(Rect{1, 1, 3, 2}));
    ASSERT_EQ_INT(1, slice_count(e));
    ASSERT_TRUE(e->sprite().slices()[0].name == "Slice 1");
    ASSERT_EQ_INT(3, e->sprite().slices()[0].rect.w);
    ASSERT_EQ_INT(0, e->selectedSlice());
    e->undo();
    ASSERT_EQ_INT(0, slice_count(e));
    ASSERT_EQ_INT(-1, e->selectedSlice()); /* nothing to select any more */
    e->redo();
    ASSERT_EQ_INT(1, slice_count(e));
    delete e;
}

TEST(a_new_slice_takes_the_smallest_unused_number)
{
    Editor *e = make_editor();
    ASSERT_TRUE(e->addSlice(Rect{0, 0, 1, 1}));
    ASSERT_TRUE(e->addSlice(Rect{1, 0, 1, 1}));
    ASSERT_TRUE(e->sprite().slices()[1].name == "Slice 2");
    ASSERT_TRUE(e->removeSlice(0));
    ASSERT_TRUE(e->addSlice(Rect{2, 0, 1, 1}));
    ASSERT_TRUE(e->sprite().slices()[1].name == "Slice 1");
    delete e;
}

TEST(a_slice_is_clipped_to_the_sprite_and_one_outside_is_refused)
{
    Editor *e = make_editor(); /* 8 x 6 */
    ASSERT_TRUE(e->addSlice(Rect{6, 4, 5, 5}));
    ASSERT_EQ_INT(2, e->sprite().slices()[0].rect.w);
    ASSERT_EQ_INT(2, e->sprite().slices()[0].rect.h);
    ASSERT_TRUE(!e->addSlice(Rect{20, 20, 2, 2}));
    ASSERT_EQ_INT(1, slice_count(e));
    delete e;
}

TEST(updating_a_slice_is_one_undo_step_and_refuses_an_invalid_one)
{
    Editor *e = make_editor();
    e->addSlice(Rect{1, 1, 2, 2});
    Sprite::Slice changed = e->sprite().slices()[0];
    changed.name = "hitbox";
    changed.rect = Rect{2, 2, 3, 3};
    changed.hasPivot = true;
    changed.pivotX = 1;
    changed.pivotY = 1;
    ASSERT_TRUE(e->updateSlice(0, changed));
    ASSERT_TRUE(e->sprite().slices()[0].name == "hitbox");
    ASSERT_TRUE(e->sprite().slices()[0].hasPivot);
    e->undo();
    ASSERT_TRUE(e->sprite().slices()[0].name == "Slice 1");
    ASSERT_TRUE(!e->sprite().slices()[0].hasPivot);
    Sprite::Slice bad = changed;
    bad.rect = Rect{7, 5, 4, 4}; /* sticks out of the sprite */
    ASSERT_TRUE(!e->updateSlice(0, bad));
    ASSERT_TRUE(!e->updateSlice(5, changed));
    delete e;
}

TEST(removing_a_slice_keeps_the_selection_on_the_same_slice)
{
    Editor *e = make_editor();
    e->addSlice(Rect{0, 0, 1, 1});
    e->addSlice(Rect{1, 0, 1, 1});
    e->addSlice(Rect{2, 0, 1, 1});
    e->selectSlice(2);
    ASSERT_TRUE(e->removeSlice(0));
    ASSERT_EQ_INT(1, e->selectedSlice()); /* still "Slice 3" */
    ASSERT_TRUE(e->sprite().slices()[1].name == "Slice 3");
    ASSERT_TRUE(e->removeSlice(1)); /* the selected one goes */
    ASSERT_EQ_INT(-1, e->selectedSlice());
    e->undo();
    ASSERT_EQ_INT(2, slice_count(e));
    ASSERT_TRUE(!e->removeSlice(9));
    delete e;
}

TEST(selecting_a_slice_out_of_range_selects_nothing)
{
    Editor *e = make_editor();
    e->addSlice(Rect{0, 0, 1, 1});
    e->selectSlice(4);
    ASSERT_EQ_INT(-1, e->selectedSlice());
    e->selectSlice(0);
    ASSERT_EQ_INT(0, e->selectedSlice());
    delete e;
}

TEST(slice_edits_are_held_off_while_dragging)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    e->press(1, 1);
    ASSERT_TRUE(!e->addSlice(Rect{0, 0, 1, 1}));
    e->release();
    ASSERT_TRUE(e->addSlice(Rect{0, 0, 1, 1}));
    delete e;
}

TEST(dragging_with_the_slice_tool_creates_a_slice)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Slice);
    e->press(1, 1);
    e->drag(4, 3);
    const std::optional<Rect> preview = e->slicePreview();
    ASSERT_TRUE(preview.has_value()); /* shown while dragging, not a slice yet */
    ASSERT_EQ_INT(0, slice_count(e));
    ASSERT_EQ_INT(4, preview ? preview->w : -1);
    e->release();
    ASSERT_TRUE(!e->slicePreview().has_value());
    ASSERT_EQ_INT(1, slice_count(e));
    ASSERT_EQ_INT(1, e->sprite().slices()[0].rect.x);
    ASSERT_EQ_INT(4, e->sprite().slices()[0].rect.w); /* both corners included */
    ASSERT_EQ_INT(3, e->sprite().slices()[0].rect.h);
    ASSERT_EQ_INT(0, e->selectedSlice());
    e->undo();
    ASSERT_EQ_INT(0, slice_count(e));
    delete e;
}

TEST(a_click_on_empty_canvas_with_the_slice_tool_creates_nothing_and_deselects)
{
    Editor *e = make_editor();
    e->addSlice(Rect{0, 0, 2, 2});
    e->selectTool(ToolKind::Slice);
    click(e, 6, 5);
    ASSERT_EQ_INT(1, slice_count(e));
    ASSERT_EQ_INT(-1, e->selectedSlice());
    ASSERT_TRUE(!e->history().canRedo());
    delete e;
}

TEST(pressing_on_a_slice_selects_it_and_dragging_moves_it)
{
    Editor *e = make_editor();
    e->addSlice(Rect{0, 0, 2, 2});
    e->addSlice(Rect{3, 1, 3, 2});
    e->selectSlice(0);
    e->selectTool(ToolKind::Slice);
    e->press(4, 1);
    ASSERT_EQ_INT(1, e->selectedSlice());
    e->drag(5, 2);                                    /* one right, one down */
    ASSERT_EQ_INT(4, e->sprite().slices()[1].rect.x); /* shows while dragging */
    e->release();
    ASSERT_EQ_INT(4, e->sprite().slices()[1].rect.x);
    ASSERT_EQ_INT(2, e->sprite().slices()[1].rect.y);
    ASSERT_EQ_INT(3, e->sprite().slices()[1].rect.w);
    e->undo(); /* the whole move is one step */
    ASSERT_EQ_INT(3, e->sprite().slices()[1].rect.x);
    ASSERT_EQ_INT(1, e->sprite().slices()[1].rect.y);
    delete e;
}

TEST(a_dragged_slice_stays_inside_the_sprite)
{
    Editor *e = make_editor(); /* 8 x 6 */
    e->addSlice(Rect{3, 1, 3, 2});
    e->selectTool(ToolKind::Slice);
    e->press(4, 1);
    e->drag(30, 30);
    e->release();
    ASSERT_EQ_INT(5, e->sprite().slices()[0].rect.x); /* 8 - 3 */
    ASSERT_EQ_INT(4, e->sprite().slices()[0].rect.y); /* 6 - 2 */
    delete e;
}

TEST(the_topmost_slice_wins_where_slices_overlap_and_a_move_that_goes_nowhere_adds_no_step)
{
    Editor *e = make_editor();
    e->addSlice(Rect{0, 0, 4, 4});
    e->addSlice(Rect{2, 2, 4, 3}); /* drawn over the first */
    e->selectTool(ToolKind::Slice);
    e->press(3, 3);
    ASSERT_EQ_INT(1, e->selectedSlice());
    e->release(); /* no movement */
    ASSERT_TRUE(!e->history().canRedo());
    e->undo(); /* undoes the second add, not a move */
    ASSERT_EQ_INT(1, slice_count(e));
    delete e;
}

TEST(cancelling_a_slice_drag_leaves_everything_as_it_was)
{
    Editor *e = make_editor();
    e->addSlice(Rect{3, 1, 3, 2});
    e->selectTool(ToolKind::Slice);
    e->press(4, 1);
    e->drag(6, 3);
    e->cancelDrag();
    ASSERT_EQ_INT(3, e->sprite().slices()[0].rect.x);
    ASSERT_EQ_INT(1, e->sprite().slices()[0].rect.y);
    e->press(0, 4);
    e->drag(2, 5);
    e->cancelDrag();
    ASSERT_EQ_INT(1, slice_count(e));
    ASSERT_TRUE(!e->slicePreview().has_value());
    delete e;
}

TEST(the_slice_tool_works_on_a_hidden_layer)
{
    Editor *e = make_editor();
    e->setLayerVisible(0, false);
    e->selectTool(ToolKind::Slice);
    e->press(1, 1);
    e->drag(3, 3);
    e->release();
    ASSERT_EQ_INT(1, slice_count(e));
    delete e;
}

int main()
{
    if (!ui_init()) {
        return 1;
    }
    RUN(adding_a_slice_names_it_selects_it_and_is_one_undo_step);
    RUN(a_new_slice_takes_the_smallest_unused_number);
    RUN(a_slice_is_clipped_to_the_sprite_and_one_outside_is_refused);
    RUN(updating_a_slice_is_one_undo_step_and_refuses_an_invalid_one);
    RUN(removing_a_slice_keeps_the_selection_on_the_same_slice);
    RUN(selecting_a_slice_out_of_range_selects_nothing);
    RUN(slice_edits_are_held_off_while_dragging);
    RUN(dragging_with_the_slice_tool_creates_a_slice);
    RUN(a_click_on_empty_canvas_with_the_slice_tool_creates_nothing_and_deselects);
    RUN(pressing_on_a_slice_selects_it_and_dragging_moves_it);
    RUN(a_dragged_slice_stays_inside_the_sprite);
    RUN(the_topmost_slice_wins_where_slices_overlap_and_a_move_that_goes_nowhere_adds_no_step);
    RUN(cancelling_a_slice_drag_leaves_everything_as_it_was);
    RUN(the_slice_tool_works_on_a_hidden_layer);
    ui_quit();
    return TEST_SUMMARY();
}
