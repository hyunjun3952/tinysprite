/* Headless tests for the UI editor: a selection of any shape (a mask), made with the magic wand or
 * the lasso, and what the clipboard, the move, flip and rotate and the drawing tools do with one.
 * Built only when the UI is built; runs under SDL_VIDEODRIVER=dummy. */

#include <utility>
#include <vector>

#include "editor.hpp"
#include "minitest.hpp"
#include "ui_editor_support.hpp"

/* A mask of exactly these cels of the 8 x 6 cel the editor tests use. */
static Mask cells(std::initializer_list<std::pair<int, int>> list)
{
    return Mask::fromPlot(8, 6, [&](const RasterPlotFn &plot) {
        for (const auto &c : list) {
            plot(c.first, c.second);
        }
    });
}

/* Red pixels at the given cels, drawn with the pencil (and the selection left as it was). */
static void paint(Editor *e, std::initializer_list<std::pair<int, int>> list)
{
    for (const auto &c : list) {
        dot(e, c.first, c.second);
    }
}

TEST(the_magic_wand_selects_the_connected_pixels_of_the_clicked_color)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {2, 1}, {3, 1}, {6, 1}});
    e->selectTool(ToolKind::Wand);
    click(e, 1, 1);
    ASSERT_TRUE(e->hasSelection());
    ASSERT_TRUE(e->selectionMask() != nullptr);
    ASSERT_EQ_INT(3, e->selectionMask()->count());
    ASSERT_TRUE(e->selection().x == 1 && e->selection().y == 1 && e->selection().w == 3 &&
                e->selection().h == 1);
    ASSERT_TRUE(!e->selectionMask()->contains(6, 1)); /* red too, but not connected */
    delete e;
}

TEST(the_wand_follows_the_bucket_options_everywhere_and_tolerance)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {2, 1}, {6, 1}});
    e->selectTool(ToolKind::Wand);
    e->options().fill_contiguous = false;
    click(e, 1, 1);
    ASSERT_EQ_INT(3, e->selectionMask()->count());
    ASSERT_TRUE(e->selectionMask()->contains(6, 1));
    e->options().fill_contiguous = true;
    e->options().fill_tolerance = 100; /* every pixel is within reach */
    click(e, 1, 1);
    ASSERT_EQ_INT(48, e->selectionMask()->count());
    delete e;
}

TEST(a_shift_click_adds_to_the_selection_and_a_plain_one_replaces_it)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {2, 1}, {6, 1}});
    e->selectTool(ToolKind::Wand);
    click(e, 1, 1);
    e->press(6, 1, true);
    e->release();
    ASSERT_EQ_INT(3, e->selectionMask()->count());
    ASSERT_TRUE(e->selection().x == 1 && e->selection().w == 6);
    click(e, 6, 1);
    ASSERT_EQ_INT(1, e->selectionMask()->count());
    delete e;
}

TEST(a_ctrl_click_takes_the_picked_pixels_out_of_the_selection)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {2, 1}, {6, 1}});
    e->selectTool(ToolKind::Wand);
    e->options().fill_contiguous = false;
    click(e, 1, 1); /* all three red pixels */
    e->options().fill_contiguous = true;
    e->press(2, 1, false, true); /* the connected pair, (1, 1) and (2, 1) */
    e->release();
    ASSERT_EQ_INT(1, e->selectionMask()->count());
    ASSERT_TRUE(e->selectionMask()->contains(6, 1));
    ASSERT_TRUE(e->selection().x == 6 && e->selection().w == 1);
    delete e;
}

TEST(subtracting_everything_drops_the_selection_and_without_one_changes_nothing)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {2, 1}});
    e->selectTool(ToolKind::Wand);
    e->press(1, 1, false, true);
    e->release();
    ASSERT_TRUE(!e->hasSelection()); /* nothing to take from */
    click(e, 1, 1);
    e->press(1, 1, false, true);
    e->release();
    ASSERT_TRUE(!e->hasSelection());
    delete e;
}

TEST(a_ctrl_click_beside_the_sprite_keeps_the_selection)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}});
    e->selectTool(ToolKind::Wand);
    click(e, 1, 1);
    e->press(-3, 2, false, true);
    e->release();
    ASSERT_TRUE(e->hasSelection());
    delete e;
}

TEST(a_ctrl_lasso_cuts_its_area_out_of_the_selection)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Lasso);
    e->press(0, 0);
    e->drag(5, 0);
    e->drag(5, 4);
    e->drag(0, 4);
    e->release();
    int before = e->selectionMask()->count();
    e->press(2, 1, false, true);
    e->drag(3, 1);
    e->drag(3, 2);
    e->drag(2, 2);
    e->release();
    ASSERT_EQ_INT(before - 4, e->selectionMask()->count());
    ASSERT_TRUE(!e->selectionMask()->contains(2, 1));
    ASSERT_TRUE(e->selectionMask()->contains(0, 0));
    delete e;
}

TEST(the_wand_on_nothing_drops_the_selection)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}});
    e->selectTool(ToolKind::Wand);
    click(e, 1, 1);
    ASSERT_TRUE(e->hasSelection());
    click(e, -3, 2); /* beside the sprite */
    ASSERT_TRUE(!e->hasSelection());
    ASSERT_TRUE(e->selectionMask() == nullptr);
    delete e;
}

TEST(the_lasso_selects_the_polygon_dragged_out)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Lasso);
    e->press(1, 1);
    e->drag(5, 1);
    e->drag(5, 4);
    e->drag(1, 4);
    e->release();
    ASSERT_TRUE(e->hasSelection());
    ASSERT_TRUE(e->selectionMask() != nullptr);
    ASSERT_TRUE(e->selectionMask()->contains(3, 2));
    ASSERT_TRUE(!e->selectionMask()->contains(6, 2) && !e->selectionMask()->contains(0, 0));
    ASSERT_TRUE(e->lassoPath().empty()); /* the preview is gone */
    delete e;
}

TEST(a_lasso_of_fewer_than_three_points_only_dismisses_the_selection)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Lasso);
    e->press(1, 1);
    e->drag(5, 1);
    e->drag(3, 4);
    e->release();
    ASSERT_TRUE(e->hasSelection());
    e->press(2, 2);
    e->drag(4, 3); /* two different cels: no area */
    e->release();
    ASSERT_TRUE(!e->hasSelection());
    e->press(1, 1);
    e->drag(5, 1);
    e->drag(3, 4);
    e->release();
    click(e, 2, 2); /* a click */
    ASSERT_TRUE(!e->hasSelection());
    e->press(1, 1);
    e->drag(5, 1);
    e->drag(3, 4);
    e->release();
    e->press(6, 5, true); /* with Shift a click picks nothing and keeps what was selected */
    e->release();
    ASSERT_TRUE(e->hasSelection());
    delete e;
}
TEST(drawing_stays_inside_the_pixels_of_the_mask)
{
    Editor *e = make_editor();
    e->setSelectionMask(cells({{1, 1}, {2, 2}}));
    e->selectTool(ToolKind::Pencil);
    e->press(0, 0);
    e->drag(3, 3);
    e->release();
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    ASSERT_EQ_INT(0, alpha(e, 0, 0));
    ASSERT_EQ_INT(0, alpha(e, 3, 3));
    e->selectTool(ToolKind::Bucket);
    click(e, 2, 1); /* inside the bounds, not in the mask */
    ASSERT_EQ_INT(0, alpha(e, 2, 1));
    ASSERT_TRUE(e->selectionMask() != nullptr); /* the stroke leaves the selection alone */
    delete e;
}

TEST(deleting_a_masked_selection_clears_only_the_masked_pixels)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {2, 1}, {3, 1}});
    e->setSelectionMask(cells({{1, 1}, {3, 1}}));
    e->deleteSelection();
    ASSERT_EQ_INT(0, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 2, 1));
    ASSERT_EQ_INT(0, alpha(e, 3, 1));
    e->undo();
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    delete e;
}

TEST(copying_a_masked_selection_takes_only_its_pixels)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {2, 1}, {3, 1}});
    e->setSelectionMask(cells({{1, 1}, {3, 1}}));
    ASSERT_TRUE(e->copySelection());
    e->setHover(4, 3, true);
    e->paste();
    ASSERT_EQ_INT(255, alpha(e, 4, 3));
    ASSERT_EQ_INT(0, alpha(e, 5, 3)); /* the unmasked middle pixel stayed behind */
    ASSERT_EQ_INT(255, alpha(e, 6, 3));
    ASSERT_TRUE(e->selectionMask() == nullptr); /* the pasted block is a plain rectangle */
    ASSERT_TRUE(e->selection().x == 4 && e->selection().w == 3);
    e->undo();
    ASSERT_EQ_INT(0, alpha(e, 4, 3));
    ASSERT_TRUE(e->selectionMask() != nullptr); /* and the old selection is back */
    delete e;
}

TEST(cutting_a_masked_selection_clears_what_it_copied)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {2, 1}, {3, 1}});
    e->setSelectionMask(cells({{1, 1}, {3, 1}}));
    ASSERT_TRUE(e->cutSelection());
    ASSERT_EQ_INT(0, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 2, 1));
    delete e;
}

TEST(nudging_a_masked_selection_moves_its_pixels_and_its_shape)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {2, 1}, {3, 1}});
    e->setSelectionMask(cells({{1, 1}, {3, 1}}));
    ASSERT_TRUE(e->nudgeSelection(0, 2));
    ASSERT_EQ_INT(0, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 2, 1)); /* not selected: stays */
    ASSERT_EQ_INT(255, alpha(e, 1, 3));
    ASSERT_EQ_INT(0, alpha(e, 2, 3));
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    ASSERT_TRUE(e->selectionMask() != nullptr);
    ASSERT_TRUE(e->selectionMask()->contains(1, 3) && e->selectionMask()->contains(3, 3));
    ASSERT_TRUE(!e->selectionMask()->contains(2, 3));
    ASSERT_EQ_INT(3, e->selection().y);
    e->undo();
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    ASSERT_EQ_INT(0, alpha(e, 1, 3));
    ASSERT_TRUE(e->selectionMask() != nullptr);
    ASSERT_EQ_INT(1, e->selection().y);
    ASSERT_TRUE(!e->selectionMask()->contains(2, 1));
    delete e;
}

TEST(dragging_a_masked_selection_with_the_select_tool_moves_only_its_pixels)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {2, 1}, {3, 1}});
    e->setSelectionMask(cells({{1, 1}, {3, 1}}));
    e->selectTool(ToolKind::Select);
    e->press(1, 1);
    e->drag(1, 3);
    e->release();
    ASSERT_EQ_INT(0, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 2, 1));
    ASSERT_EQ_INT(255, alpha(e, 1, 3));
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    ASSERT_TRUE(e->selectionMask() != nullptr && e->selectionMask()->contains(3, 3));
    e->undo();
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    ASSERT_TRUE(e->selectionMask() != nullptr && e->selectionMask()->contains(3, 1));
    delete e;
}

TEST(a_press_inside_the_bounds_but_outside_the_mask_does_not_move_anything)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {2, 1}, {3, 1}});
    e->setSelectionMask(cells({{1, 1}, {3, 1}}));
    e->selectTool(ToolKind::Select);
    click(e, 2, 1); /* the gap: a plain click there dismisses the selection */
    ASSERT_TRUE(!e->hasSelection());
    ASSERT_EQ_INT(255, alpha(e, 2, 1));
    delete e;
}

TEST(flipping_a_masked_selection_mirrors_its_pixels_and_its_shape)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {1, 2}, {2, 2}, {2, 1}});
    e->setSelectionMask(cells({{1, 1}, {1, 2}, {2, 2}})); /* the pixel at (2, 1) is not selected */
    e->flipSelection(true);
    ASSERT_EQ_INT(0, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 2, 1)); /* (1,1) landed here, on the pixel that was there */
    ASSERT_EQ_INT(255, alpha(e, 1, 2));
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    ASSERT_TRUE(e->selectionMask() != nullptr);
    ASSERT_TRUE(e->selectionMask()->contains(2, 1) && e->selectionMask()->contains(1, 2) &&
                e->selectionMask()->contains(2, 2));
    ASSERT_TRUE(!e->selectionMask()->contains(1, 1));
    e->undo();
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    ASSERT_TRUE(e->selectionMask()->contains(1, 1) && !e->selectionMask()->contains(2, 1));
    delete e;
}

TEST(rotating_a_masked_selection_turns_its_pixels_and_its_shape)
{
    Editor *e = make_editor();
    paint(e, {{1, 1}, {1, 2}, {1, 3}, {2, 3}, {3, 3}}); /* an L: "#.." "#.." "###" */
    e->setSelectionMask(cells({{1, 1}, {1, 2}, {1, 3}, {2, 3}, {3, 3}}));
    e->rotateSelection(true); /* clockwise: "###" "#.." "#.." */
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 2, 1));
    ASSERT_EQ_INT(255, alpha(e, 3, 1));
    ASSERT_EQ_INT(255, alpha(e, 1, 2));
    ASSERT_EQ_INT(255, alpha(e, 1, 3));
    ASSERT_EQ_INT(0, alpha(e, 2, 3));
    ASSERT_EQ_INT(0, alpha(e, 3, 3));
    ASSERT_TRUE(e->selectionMask() != nullptr);
    ASSERT_TRUE(e->selectionMask()->contains(3, 1) && !e->selectionMask()->contains(3, 3));
    e->undo();
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    ASSERT_TRUE(e->selectionMask()->contains(3, 3) && !e->selectionMask()->contains(3, 1));
    delete e;
}

TEST(a_new_rectangular_selection_or_select_all_or_dismissing_drops_the_mask)
{
    Editor *e = make_editor();
    e->setSelectionMask(cells({{1, 1}, {2, 2}}));
    e->selectAll();
    ASSERT_TRUE(e->hasSelection() && e->selectionMask() == nullptr);
    e->setSelectionMask(cells({{1, 1}, {2, 2}}));
    e->clearSelection();
    ASSERT_TRUE(!e->hasSelection() && e->selectionMask() == nullptr);
    e->setSelectionMask(cells({{1, 1}, {2, 2}}));
    e->selectTool(ToolKind::Select);
    e->press(5, 4);
    e->drag(6, 5);
    e->release();
    ASSERT_TRUE(e->hasSelection() && e->selectionMask() == nullptr);
    ASSERT_TRUE(e->selection().x == 5 && e->selection().w == 2);
    delete e;
}

TEST(an_empty_mask_is_no_selection)
{
    Editor *e = make_editor();
    e->setSelectionMask(cells({{1, 1}}));
    e->setSelectionMask(Mask());
    ASSERT_TRUE(!e->hasSelection() && e->selectionMask() == nullptr);
    delete e;
}

int main()
{
    if (!ui_init()) {
        return 1;
    }
    RUN(the_magic_wand_selects_the_connected_pixels_of_the_clicked_color);
    RUN(the_wand_follows_the_bucket_options_everywhere_and_tolerance);
    RUN(a_shift_click_adds_to_the_selection_and_a_plain_one_replaces_it);
    RUN(a_ctrl_click_takes_the_picked_pixels_out_of_the_selection);
    RUN(subtracting_everything_drops_the_selection_and_without_one_changes_nothing);
    RUN(a_ctrl_click_beside_the_sprite_keeps_the_selection);
    RUN(a_ctrl_lasso_cuts_its_area_out_of_the_selection);
    RUN(the_wand_on_nothing_drops_the_selection);
    RUN(the_lasso_selects_the_polygon_dragged_out);
    RUN(a_lasso_of_fewer_than_three_points_only_dismisses_the_selection);
    RUN(drawing_stays_inside_the_pixels_of_the_mask);
    RUN(deleting_a_masked_selection_clears_only_the_masked_pixels);
    RUN(copying_a_masked_selection_takes_only_its_pixels);
    RUN(cutting_a_masked_selection_clears_what_it_copied);
    RUN(nudging_a_masked_selection_moves_its_pixels_and_its_shape);
    RUN(dragging_a_masked_selection_with_the_select_tool_moves_only_its_pixels);
    RUN(a_press_inside_the_bounds_but_outside_the_mask_does_not_move_anything);
    RUN(flipping_a_masked_selection_mirrors_its_pixels_and_its_shape);
    RUN(rotating_a_masked_selection_turns_its_pixels_and_its_shape);
    RUN(a_new_rectangular_selection_or_select_all_or_dismissing_drops_the_mask);
    RUN(an_empty_mask_is_no_selection);
    ui_quit();
    return TEST_SUMMARY();
}
