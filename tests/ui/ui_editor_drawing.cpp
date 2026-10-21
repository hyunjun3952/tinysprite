/* Headless tests for the UI editor state: the drawing tools: brush, shift constraints, symmetry,
 * temporary eyedropper, tool switching. Built only when the UI is built; runs under
 * SDL_VIDEODRIVER=dummy. */

#include "editor.h"
#include "minitest.h"
#include "ui_editor_support.h"

TEST(mirrored_shape_and_bucket_are_symmetric)
{
    Editor *e = make_editor();
    e->options().symmetry.mirror_x = true;
    e->selectTool(ToolKind::Line);
    e->press(0, 0);
    e->drag(2, 0);
    e->release();
    ASSERT_EQ_INT(255, alpha(e, 7, 0));
    ASSERT_EQ_INT(255, alpha(e, 5, 0));
    ASSERT_EQ_INT(0, alpha(e, 4, 0));

    e->undo();
    e->selectTool(ToolKind::Rect);
    e->press(0, 0);
    e->drag(7, 0);
    e->release();
    e->selectTool(ToolKind::Bucket);
    click(e, 0, 3);
    ASSERT_EQ_INT(255, alpha(e, 0, 5));
    ASSERT_EQ_INT(255, alpha(e, 7, 5));
    delete e;
}

TEST(brush_size_steps_and_stays_within_its_limits)
{
    Editor *e = make_editor();
    ASSERT_EQ_INT(1, e->options().brush_size);
    e->changeBrushSize(1);
    ASSERT_EQ_INT(2, e->options().brush_size);
    e->changeBrushSize(5);
    ASSERT_EQ_INT(7, e->options().brush_size);
    e->changeBrushSize(-1);
    ASSERT_EQ_INT(6, e->options().brush_size);
    e->changeBrushSize(-5);
    ASSERT_EQ_INT(1, e->options().brush_size);
    e->changeBrushSize(-5); /* never below 1 */
    ASSERT_EQ_INT(1, e->options().brush_size);
    e->changeBrushSize(1000); /* never above the maximum */
    ASSERT_EQ_INT(PENCIL_BRUSH_MAX, e->options().brush_size);
    e->changeBrushSize(1);
    ASSERT_EQ_INT(PENCIL_BRUSH_MAX, e->options().brush_size);
    delete e;
}

TEST(temp_pick_switches_to_the_picker_and_back)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Rect);
    e->tempPick(1);
    ASSERT_EQ_INT(ToolKind::Eyedropper, e->tool());
    e->tempPick(1); /* held: stays on the picker, remembers the first tool */
    e->tempPick(0);
    ASSERT_EQ_INT(ToolKind::Rect, e->tool());
    e->tempPick(0);
    ASSERT_EQ_INT(ToolKind::Rect, e->tool());
    delete e;
}

TEST(temp_pick_keeps_the_picker_when_it_is_already_active)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Eyedropper);
    e->tempPick(1);
    e->tempPick(0);
    ASSERT_EQ_INT(ToolKind::Eyedropper, e->tool());
    delete e;
}

TEST(selecting_a_tool_ends_a_temp_pick)
{
    Editor *e = make_editor();
    e->tempPick(1);
    e->selectTool(ToolKind::Bucket);
    e->tempPick(0);
    ASSERT_EQ_INT(ToolKind::Bucket, e->tool());
    delete e;
}

TEST(tool_changes_are_held_off_while_dragging)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    e->press(0, 0);
    e->selectTool(ToolKind::Bucket);
    e->tempPick(1);
    ASSERT_EQ_INT(ToolKind::Pencil, e->tool());
    e->release();
    e->tempPick(1);
    ASSERT_EQ_INT(ToolKind::Eyedropper, e->tool());
    delete e;
}

TEST(shift_press_joins_the_last_pencil_point_with_a_line)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    click(e, 1, 1);
    e->press(5, 1, true);
    e->release();
    for (int x = 1; x <= 5; x++) {
        ASSERT_EQ_INT(255, alpha(e, x, 1));
    }
    e->undo();
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    ASSERT_EQ_INT(0, alpha(e, 3, 1));
    ASSERT_EQ_INT(0, alpha(e, 5, 1));
    delete e;
}

TEST(shift_press_without_a_previous_point_draws_a_dot)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    e->press(2, 2, true);
    e->release();
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    ASSERT_EQ_INT(0, alpha(e, 1, 2));
    delete e;
}

TEST(shift_line_chains_from_its_own_end_and_resets_with_the_document)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    click(e, 0, 0);
    e->press(3, 0, true);
    e->release();
    e->press(3, 3, true);
    e->release();
    ASSERT_EQ_INT(255, alpha(e, 3, 2));
    ASSERT_TRUE(e->setSprite(Sprite(8, 8)));
    e->press(5, 5, true);
    e->release();
    ASSERT_EQ_INT(255, alpha(e, 5, 5));
    ASSERT_EQ_INT(0, alpha(e, 0, 0));
    delete e;
}

TEST(pencil_uses_the_brush_size_and_marks_it_dirty)
{
    Editor *e = make_editor();
    e->options().brush_size = 3;
    e->selectTool(ToolKind::Pencil);
    e->press(3, 2);
    e->release();
    ASSERT_EQ_INT(255, alpha(e, 2, 1));
    ASSERT_EQ_INT(255, alpha(e, 4, 3));
    ASSERT_EQ_INT(0, alpha(e, 5, 3));
    ASSERT_TRUE(e->dirty());
    ASSERT_TRUE(e->dirtyRect().x <= 2 && e->dirtyRect().y <= 1);
    ASSERT_TRUE(e->dirtyRect().x + e->dirtyRect().w > 4 && e->dirtyRect().y + e->dirtyRect().h > 3);
    e->undo();
    ASSERT_EQ_INT(0, alpha(e, 3, 2));
    delete e;
}

TEST(round_brush_and_eraser_follow_the_options)
{
    Editor *e = make_editor();
    e->options().brush_size = 3;
    e->options().brush_round = true;
    e->selectTool(ToolKind::Pencil);
    click(e, 3, 2);
    ASSERT_EQ_INT(0, alpha(e, 2, 1)); /* corner cut off */
    ASSERT_EQ_INT(255, alpha(e, 3, 1));
    e->selectTool(ToolKind::Eraser);
    e->options().brush_size = 1;
    click(e, 3, 2);
    ASSERT_EQ_INT(0, alpha(e, 3, 2));
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    delete e;
}

TEST(brush_size_widens_a_line_and_the_outline_of_a_shape_but_not_a_fill)
{
    Editor *e = make_editor();
    e->options().brush_size = 3;
    e->selectTool(ToolKind::Line);
    e->press(2, 2);
    e->drag(5, 2);
    e->release();
    ASSERT_EQ_INT(255, alpha(e, 3, 1)); /* a 3 px wide line */
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    ASSERT_EQ_INT(0, alpha(e, 3, 0));
    e->undo();
    ASSERT_EQ_INT(0, alpha(e, 3, 1));

    e->options().shape_filled = true;
    e->selectTool(ToolKind::Rect);
    e->press(2, 2);
    e->drag(4, 3);
    e->release();
    ASSERT_EQ_INT(255, alpha(e, 4, 3));
    ASSERT_EQ_INT(0, alpha(e, 5, 3)); /* the filled box ignores the brush */
    ASSERT_EQ_INT(0, alpha(e, 2, 1));
    delete e;
}

TEST(a_thick_shape_marks_its_whole_outline_dirty)
{
    Editor *e = make_editor();
    e->updateTexture(); /* clears the initial full dirty rect */
    e->options().brush_size = 3;
    e->selectTool(ToolKind::Line);
    e->press(3, 3);
    e->drag(5, 3);
    e->release();
    Rect dirty = e->dirtyRect();
    ASSERT_TRUE(dirty.y <= 2 && dirty.y + dirty.h > 4);
    delete e;
}

TEST(mirrored_stroke_marks_only_the_reflected_area_dirty)
{
    Editor *e = make_editor(); /* 8 x 6 */
    e->options().symmetry.mirror_x = true;
    e->selectTool(ToolKind::Pencil);
    e->updateTexture(); /* clears the initial full dirty rect */
    e->press(0, 0);
    e->release();
    ASSERT_TRUE(e->dirty());
    Rect r = e->dirtyRect();
    ASSERT_TRUE(rect_contains(r, 0, 0));
    ASSERT_TRUE(rect_contains(r, 7, 0));  /* the reflected pixel */
    ASSERT_TRUE(!rect_contains(r, 0, 5)); /* but not the whole sprite */
    delete e;
}

TEST(symmetry_is_off_by_default)
{
    Editor *e = make_editor();
    ASSERT_TRUE(!e->options().symmetry.mirror_x);
    ASSERT_TRUE(!e->options().symmetry.mirror_y);
    delete e;
}

TEST(shift_keeps_a_dragged_rectangle_square)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Rect);
    e->press(1, 1);
    e->drag(5, 3, true); /* 4 wide, 2 tall: Shift makes it 4 x 4 */
    e->release();
    ASSERT_EQ_INT(255, alpha(e, 5, 5));
    ASSERT_EQ_INT(255, alpha(e, 1, 5));
    ASSERT_EQ_INT(0, alpha(e, 3, 3)); /* an outline */
    delete e;
}

TEST(shift_snaps_a_dragged_line_and_can_be_let_go)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Line);
    e->press(1, 1);
    e->drag(6, 2, true); /* nearly flat: snaps to horizontal */
    ASSERT_EQ_INT(255, alpha(e, 6, 1));
    ASSERT_EQ_INT(0, alpha(e, 6, 2));
    e->drag(6, 2, false); /* released Shift: the cursor again */
    ASSERT_EQ_INT(255, alpha(e, 6, 2));
    ASSERT_EQ_INT(0, alpha(e, 6, 1));
    e->release();
    delete e;
}

int main(void)
{
    if (!ui_init()) {
        return 1;
    }
    RUN(mirrored_shape_and_bucket_are_symmetric);
    RUN(brush_size_steps_and_stays_within_its_limits);
    RUN(temp_pick_switches_to_the_picker_and_back);
    RUN(temp_pick_keeps_the_picker_when_it_is_already_active);
    RUN(selecting_a_tool_ends_a_temp_pick);
    RUN(tool_changes_are_held_off_while_dragging);
    RUN(shift_press_joins_the_last_pencil_point_with_a_line);
    RUN(shift_press_without_a_previous_point_draws_a_dot);
    RUN(shift_line_chains_from_its_own_end_and_resets_with_the_document);
    RUN(pencil_uses_the_brush_size_and_marks_it_dirty);
    RUN(round_brush_and_eraser_follow_the_options);
    RUN(brush_size_widens_a_line_and_the_outline_of_a_shape_but_not_a_fill);
    RUN(a_thick_shape_marks_its_whole_outline_dirty);
    RUN(mirrored_stroke_marks_only_the_reflected_area_dirty);
    RUN(symmetry_is_off_by_default);
    RUN(shift_keeps_a_dragged_rectangle_square);
    RUN(shift_snaps_a_dragged_line_and_can_be_let_go);
    ui_quit();
    return TEST_SUMMARY();
}
