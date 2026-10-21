/* Headless tests for the UI editor state: the selection tool, the clipboard (copy / cut / paste)
 * and selection flip / rotate. Built only when the UI is built; runs under SDL_VIDEODRIVER=dummy.
 */

#include "editor.h"
#include "minitest.h"
#include "ui_editor_support.h"

static void select_rect(Editor *e, int x0, int y0, int x1, int y1)
{
    e->selectTool(ToolKind::Select);
    e->press(x0, y0);
    e->drag(x1, y1);
    e->release();
}

TEST(select_click_on_empty_canvas_selects_one_cel)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Select);
    click(e, 3, 2);
    ASSERT_TRUE(e->hasSelection());
    ASSERT_EQ_INT(1, e->selection().w);
    ASSERT_EQ_INT(1, e->selection().h);
    ASSERT_EQ_INT(3, e->selection().x);
    ASSERT_EQ_INT(2, e->selection().y);
    delete e;
}

TEST(select_click_outside_a_selection_only_dismisses_it)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Select);
    e->press(1, 1);
    e->drag(3, 3);
    e->release();
    ASSERT_EQ_INT(3, e->selection().w);
    click(e, 6, 5);
    ASSERT_TRUE(!e->hasSelection());
    click(e, 6, 5);
    ASSERT_TRUE(e->hasSelection());
    delete e;
}

TEST(drawing_stays_inside_the_selection)
{
    Editor *e = make_editor();
    select_rect(e, 1, 1, 2, 2);
    e->selectTool(ToolKind::Pencil);
    click(e, 0, 0);
    ASSERT_EQ_INT(0, alpha(e, 0, 0));
    e->press(0, 1);
    e->drag(3, 1);
    e->release();
    ASSERT_EQ_INT(0, alpha(e, 0, 1));
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 2, 1));
    ASSERT_EQ_INT(0, alpha(e, 3, 1));
    e->selectTool(ToolKind::Bucket);
    click(e, 2, 2);
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    ASSERT_EQ_INT(0, alpha(e, 0, 3));
    ASSERT_TRUE(e->hasSelection());
    delete e;
}

TEST(clip_is_lifted_after_the_stroke)
{
    Editor *e = make_editor();
    select_rect(e, 1, 1, 2, 2);
    e->selectTool(ToolKind::Pencil);
    click(e, 1, 1);
    e->clearSelection();
    click(e, 0, 0);
    ASSERT_EQ_INT(255, alpha(e, 0, 0));
    delete e;
}

TEST(copy_and_paste_at_the_hovered_cel)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    dot(e, 2, 1);
    select_rect(e, 1, 1, 2, 1);
    ASSERT_TRUE(e->copySelection());
    e->setHover(4, 3, 1);
    e->paste();
    ASSERT_EQ_INT(255, alpha(e, 4, 3));
    ASSERT_EQ_INT(255, alpha(e, 5, 3));
    ASSERT_EQ_INT(255, alpha(e, 1, 1)); /* copying leaves the source alone */
    ASSERT_EQ_INT(ToolKind::Select, e->tool());
    ASSERT_TRUE(e->hasSelection());
    ASSERT_EQ_INT(4, e->selection().x);
    ASSERT_EQ_INT(3, e->selection().y);
    ASSERT_EQ_INT(2, e->selection().w);
    e->undo();
    ASSERT_EQ_INT(0, alpha(e, 4, 3));
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    delete e;
}

TEST(paste_without_hover_uses_the_selection_corner)
{
    Editor *e = make_editor();
    dot(e, 3, 3);
    select_rect(e, 3, 3, 3, 3);
    e->copySelection();
    select_rect(e, 5, 4, 6, 5);
    e->paste();
    ASSERT_EQ_INT(255, alpha(e, 5, 4));
    e->clearSelection();
    e->undo(); /* brings the earlier selection back with the pixels */
    e->clearSelection();
    e->paste(); /* no selection and no hover: top-left of the sprite */
    ASSERT_EQ_INT(255, alpha(e, 0, 0));
    delete e;
}

TEST(paste_without_a_clipboard_does_nothing)
{
    Editor *e = make_editor();
    e->paste();
    ASSERT_TRUE(!e->history().canUndo());
    ASSERT_TRUE(!e->copySelection()); /* nothing selected */
    delete e;
}

TEST(cut_clears_the_selection_and_can_be_pasted_elsewhere)
{
    Editor *e = make_editor();
    dot(e, 2, 2);
    select_rect(e, 2, 2, 3, 3);
    ASSERT_TRUE(e->cutSelection());
    ASSERT_EQ_INT(0, alpha(e, 2, 2));
    e->setHover(6, 0, 1);
    e->paste();
    ASSERT_EQ_INT(255, alpha(e, 6, 0));
    e->undo();
    e->undo(); /* the cut */
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    delete e;
}

TEST(paste_and_cut_ignore_a_hidden_layer)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    select_rect(e, 1, 1, 1, 1);
    e->copySelection();
    e->setLayerVisible(0, 0);
    ASSERT_TRUE(!e->cutSelection());
    e->paste();
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    e->setLayerVisible(0, 1);
    delete e;
}

TEST(flip_selection_mirrors_only_inside_the_selection)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    dot(e, 5, 1); /* outside the selection */
    select_rect(e, 1, 1, 3, 2);
    e->flipSelection(1);
    ASSERT_EQ_INT(0, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 3, 1));
    ASSERT_EQ_INT(255, alpha(e, 5, 1));
    e->flipSelection(0);
    ASSERT_EQ_INT(255, alpha(e, 3, 2));
    ASSERT_EQ_INT(0, alpha(e, 3, 1));
    e->undo();
    e->undo();
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    ASSERT_EQ_INT(1, e->selection().x);
    delete e;
}

TEST(rotate_selection_turns_it_around_its_center)
{
    Editor *e = make_editor();
    dot(e, 1, 2);
    select_rect(e, 1, 2, 3, 2); /* a 3 x 1 strip with the pixel at its left end */
    e->rotateSelection(1);
    ASSERT_EQ_INT(1, e->selection().w);
    ASSERT_EQ_INT(3, e->selection().h);
    ASSERT_EQ_INT(2, e->selection().x);
    ASSERT_EQ_INT(1, e->selection().y);
    ASSERT_EQ_INT(0, alpha(e, 1, 2));
    ASSERT_EQ_INT(255, alpha(e, 2, 1)); /* clockwise: the left end goes to the top */
    e->undo();
    ASSERT_EQ_INT(255, alpha(e, 1, 2));
    ASSERT_EQ_INT(0, alpha(e, 2, 1));
    delete e;
}

TEST(rotate_selection_near_the_edge_keeps_what_fits)
{
    Editor *e = make_editor();
    dot(e, 0, 0);
    dot(e, 1, 0);
    dot(e, 2, 0);
    select_rect(e, 0, 0, 2, 0);
    e->rotateSelection(1);
    ASSERT_TRUE(e->hasSelection());
    ASSERT_EQ_INT(0, e->selection().y);
    ASSERT_EQ_INT(2, e->selection().h); /* the top row of the turned strip fell off the sprite */
    ASSERT_EQ_INT(255, alpha(e, 1, 0));
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    delete e;
}

TEST(selection_transforms_are_held_off_while_dragging)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    select_rect(e, 1, 1, 2, 2);
    e->selectTool(ToolKind::Pencil);
    e->press(6, 5);
    e->flipSelection(1);
    e->rotateSelection(1);
    ASSERT_TRUE(!e->copySelection());
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    e->release();
    delete e;
}

TEST(pressing_beside_the_sprite_records_no_color_and_selects_nothing)
{
    Editor *e = make_editor();
    click(e, -5, -5);
    ASSERT_EQ_INT(0, e->recent().size());
    e->selectTool(ToolKind::Select);
    click(e, 50, 50);
    ASSERT_TRUE(!e->hasSelection());
    delete e;
}

TEST(select_inside_a_selection_on_a_locked_layer_starts_a_new_selection)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Select);
    e->selectAll();
    e->sprite().layer(0)->setLocked(true);
    e->press(2, 2);
    ASSERT_TRUE(e->stroking());
    e->release();
    delete e;
}

TEST(undoing_a_rotate_brings_back_the_selection_rectangle)
{
    Editor *e = make_editor();
    dot(e, 2, 2);
    select_rect(e, 1, 2, 4, 3); /* 4 wide, 2 tall */
    Rect before = e->selection();
    e->rotateSelection(true);
    Rect turned = e->selection();
    ASSERT_TRUE(turned.w != before.w);
    e->undo();
    ASSERT_TRUE(e->hasSelection());
    ASSERT_EQ_INT(before.x, e->selection().x);
    ASSERT_EQ_INT(before.w, e->selection().w);
    ASSERT_EQ_INT(before.h, e->selection().h);
    e->redo();
    ASSERT_EQ_INT(turned.w, e->selection().w);
    ASSERT_EQ_INT(turned.x, e->selection().x);
    delete e;
}

TEST(rotating_an_empty_selection_is_still_undoable)
{
    Editor *e = make_editor();
    select_rect(e, 1, 1, 4, 2); /* nothing painted */
    Rect before = e->selection();
    e->rotateSelection(false);
    ASSERT_TRUE(e->selection().w != before.w);
    e->undo();
    ASSERT_EQ_INT(before.w, e->selection().w);
    delete e;
}

TEST(undoing_a_flip_selects_the_flipped_area_again)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    select_rect(e, 0, 0, 2, 2);
    e->flipSelection(true);
    select_rect(e, 5, 3, 6, 4); /* the user picks another area */
    e->undo();
    ASSERT_TRUE(e->hasSelection());
    ASSERT_EQ_INT(0, e->selection().x);
    ASSERT_EQ_INT(3, e->selection().w);
    delete e;
}

TEST(undoing_a_paste_restores_the_previous_selection)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    select_rect(e, 1, 1, 1, 1);
    e->copySelection();
    e->clearSelection();
    e->setHover(5, 3, true);
    e->paste();
    ASSERT_TRUE(e->hasSelection());
    e->undo();
    ASSERT_TRUE(!e->hasSelection());
    e->redo();
    ASSERT_EQ_INT(5, e->selection().x);
    delete e;
}

TEST(undoing_a_move_puts_the_selection_back)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    select_rect(e, 0, 0, 2, 2);
    e->press(1, 1);
    e->drag(4, 3);
    e->release();
    ASSERT_EQ_INT(3, e->selection().x);
    e->undo();
    ASSERT_EQ_INT(0, e->selection().x);
    ASSERT_EQ_INT(0, e->selection().y);
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    e->redo();
    ASSERT_EQ_INT(3, e->selection().x);
    ASSERT_EQ_INT(2, e->selection().y);
    delete e;
}

TEST(a_click_that_moves_nothing_adds_no_undo_step)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    select_rect(e, 0, 0, 2, 2);
    e->undo(); /* the dot */
    e->redo();
    e->press(1, 1);
    e->release();
    e->undo();
    ASSERT_EQ_INT(0, alpha(e, 1, 1)); /* this undid the dot, not a phantom move */
    delete e;
}

int main(void)
{
    if (!ui_init()) {
        return 1;
    }
    RUN(select_click_on_empty_canvas_selects_one_cel);
    RUN(select_click_outside_a_selection_only_dismisses_it);
    RUN(drawing_stays_inside_the_selection);
    RUN(clip_is_lifted_after_the_stroke);
    RUN(copy_and_paste_at_the_hovered_cel);
    RUN(paste_without_hover_uses_the_selection_corner);
    RUN(paste_without_a_clipboard_does_nothing);
    RUN(cut_clears_the_selection_and_can_be_pasted_elsewhere);
    RUN(paste_and_cut_ignore_a_hidden_layer);
    RUN(flip_selection_mirrors_only_inside_the_selection);
    RUN(rotate_selection_turns_it_around_its_center);
    RUN(rotate_selection_near_the_edge_keeps_what_fits);
    RUN(selection_transforms_are_held_off_while_dragging);
    RUN(pressing_beside_the_sprite_records_no_color_and_selects_nothing);
    RUN(select_inside_a_selection_on_a_locked_layer_starts_a_new_selection);
    RUN(undoing_a_rotate_brings_back_the_selection_rectangle);
    RUN(rotating_an_empty_selection_is_still_undoable);
    RUN(undoing_a_flip_selects_the_flipped_area_again);
    RUN(undoing_a_paste_restores_the_previous_selection);
    RUN(undoing_a_move_puts_the_selection_back);
    RUN(a_click_that_moves_nothing_adds_no_undo_step);
    ui_quit();
    return TEST_SUMMARY();
}
