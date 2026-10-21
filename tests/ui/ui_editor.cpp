/* Headless tests for the UI's editor state and tool handlers (no ImGui frames, SDL dummy video).
 * Built only with -DTINYSPRITE_BUILD_UI=ON. */
#include <cstdlib>
#include <cstring>

#include "editor.h"
#include "minitest.h"

static SDL_Window *window;
static SDL_Renderer *renderer;

static Editor *make_editor(void)
{
    Editor *e = Editor::create(renderer, 8, 6).release();
    e->foregroundFloats()[0] = 1.0f;
    e->foregroundFloats()[3] = 1.0f; /* opaque red */
    return e;
}

static int alpha(Editor *e, int x, int y)
{
    return e->activeCel()->get(x, y).a;
}

static void click(Editor *e, int x, int y)
{
    e->press(x, y);
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

TEST(layer_add_remove_move_undo_redo)
{
    Editor *e = make_editor();
    e->addLayer();
    e->addLayer();
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    ASSERT_EQ_INT(2, e->activeLayer());
    e->selectTool(ToolKind::Pencil);
    click(e, 1, 1);
    e->removeLayer(2);
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    ASSERT_EQ_INT(1, e->activeLayer());
    e->undo();
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    e->undo(); /* the pixel on the restored layer */
    e->undo(); /* second add */
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    ASSERT_EQ_INT(1, e->activeLayer());
    e->undo();
    ASSERT_EQ_INT(1, e->sprite().layerCount());
    ASSERT_EQ_INT(0, e->activeLayer());
    e->redo();
    e->redo();
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    delete e;
}

TEST(visibility_toggle_is_undoable)
{
    Editor *e = make_editor();
    e->setLayerVisible(0, 0);
    ASSERT_TRUE(!e->sprite().layer(0)->visible());
    e->undo();
    ASSERT_TRUE(e->sprite().layer(0)->visible());
    delete e;
}

TEST(live_name_and_opacity_edits_commit_as_one_step)
{
    Editor *e = make_editor();
    Layer *layer = e->sprite().layer(0);
    layer->setName("B");
    layer->setName("Bo");
    layer->setName("Body");
    e->commitLayerName(0, "Layer 1");
    layer->setOpacity(200);
    layer->setOpacity(90);
    e->commitLayerOpacity(0, 255);
    ASSERT_TRUE(strcmp(layer->name().c_str(), "Body") == 0);
    ASSERT_EQ_INT(90, layer->opacity());
    e->undo();
    ASSERT_EQ_INT(255, layer->opacity());
    e->undo();
    ASSERT_TRUE(strcmp(layer->name().c_str(), "Layer 1") == 0);
    ASSERT_TRUE(!e->history().canUndo());
    e->redo();
    ASSERT_TRUE(strcmp(layer->name().c_str(), "Body") == 0);
    delete e;
}

TEST(unchanged_live_edit_records_nothing)
{
    Editor *e = make_editor();
    e->commitLayerName(0, "Layer 1");
    e->commitLayerOpacity(0, 255);
    ASSERT_TRUE(!e->history().canUndo());
    delete e;
}

TEST(layer_edits_are_held_off_while_dragging)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    e->press(0, 0);
    e->addLayer();
    e->setLayerVisible(0, 0);
    ASSERT_EQ_INT(1, e->sprite().layerCount());
    ASSERT_TRUE(e->sprite().layer(0)->visible());
    e->release();
    delete e;
}

TEST(tools_ignore_a_hidden_layer)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    e->setLayerVisible(0, 0);
    click(e, 2, 2);
    ASSERT_EQ_INT(0, alpha(e, 2, 2));
    delete e;
}

TEST(swap_colors_exchanges_foreground_and_background)
{
    Editor *e = make_editor();
    ASSERT_EQ_INT(255, e->foreground().r);
    ASSERT_EQ_INT(0, e->foreground().g);
    e->swapColors();
    ASSERT_EQ_INT(255, e->foreground().g);
    ASSERT_EQ_INT(255, e->backgroundFloats()[0] > 0.5f ? 255 : 0);
    ASSERT_EQ_INT(0, e->backgroundFloats()[1] > 0.5f ? 255 : 0);
    e->swapColors();
    ASSERT_EQ_INT(0, e->foreground().g);
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

/* Draws a red pixel at each of the given cels with the pencil. */
static void dot(Editor *e, int x, int y)
{
    e->selectTool(ToolKind::Pencil);
    click(e, x, y);
}

static void select_rect(Editor *e, int x0, int y0, int x1, int y1)
{
    e->selectTool(ToolKind::Select);
    e->press(x0, y0);
    e->drag(x1, y1);
    e->release();
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
    e->undo();
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

TEST(drawing_records_recent_colors)
{
    Editor *e = make_editor();
    ASSERT_EQ_INT(0, e->recent().size());
    dot(e, 0, 0);
    ASSERT_EQ_INT(1, e->recent().size());
    ASSERT_EQ_INT(255, e->recent()[0].r);
    dot(e, 1, 0); /* the same color again */
    ASSERT_EQ_INT(1, e->recent().size());

    e->setForeground(Rgba{0, 0, 255, 255});
    e->selectTool(ToolKind::Bucket);
    click(e, 5, 5);
    ASSERT_EQ_INT(2, e->recent().size());
    ASSERT_EQ_INT(255, e->recent()[0].b);

    e->setForeground(Rgba{0, 255, 0, 255});
    e->selectTool(ToolKind::Rect);
    e->press(2, 2);
    e->drag(3, 3);
    e->release();
    ASSERT_EQ_INT(3, e->recent().size());
    delete e;
}

TEST(eraser_and_picker_do_not_record_colors)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Eraser);
    click(e, 0, 0);
    e->selectTool(ToolKind::Eyedropper);
    click(e, 0, 0);
    ASSERT_EQ_INT(0, e->recent().size());
    delete e;
}

TEST(set_color_changes_the_foreground)
{
    Editor *e = make_editor();
    e->setForeground(Rgba{0, 128, 255, 51});
    Rgba c = e->foreground();
    ASSERT_EQ_INT(0, c.r);
    ASSERT_EQ_INT(128, c.g);
    ASSERT_EQ_INT(255, c.b);
    ASSERT_EQ_INT(51, c.a);
    delete e;
}

TEST(palette_starts_with_default_colors)
{
    Editor *e = make_editor();
    ASSERT_TRUE(e->sprite().palette().count() > 0);
    delete e;
}

TEST(palette_add_uses_the_current_color_once)
{
    Editor *e = make_editor();
    Palette &p = e->sprite().palette();
    int before = p.count();
    e->setForeground(Rgba{12, 34, 56, 255});
    int index = e->paletteAdd();
    ASSERT_EQ_INT(before, index);
    ASSERT_EQ_INT(before + 1, p.count());
    ASSERT_EQ_INT(index, e->paletteSelected());
    ASSERT_EQ_INT(56, p.get(index).b);
    ASSERT_EQ_INT(index, e->paletteAdd()); /* already there: no duplicate */
    ASSERT_EQ_INT(before + 1, p.count());
    delete e;
}

TEST(palette_remove_keeps_the_selection_on_the_same_color)
{
    Editor *e = make_editor();
    Palette &p = e->sprite().palette();
    int count = p.count();
    e->palettePick(count - 1);
    Rgba last = p.get(count - 1);
    e->paletteRemove(0);
    ASSERT_EQ_INT(count - 1, p.count());
    ASSERT_EQ_INT(count - 2, e->paletteSelected());
    ASSERT_EQ_INT(last.r, p.get(e->paletteSelected()).r);
    e->paletteRemove(e->paletteSelected()); /* removing the selected swatch deselects */
    ASSERT_TRUE(e->paletteSelected() < p.count());
    e->paletteRemove(999);
    ASSERT_EQ_INT(count - 2, p.count());
    delete e;
}

TEST(pick_palette_color_sets_the_foreground)
{
    Editor *e = make_editor();
    Palette &p = e->sprite().palette();
    Rgba want = p.get(2);
    e->palettePick(2);
    ASSERT_EQ_INT(2, e->paletteSelected());
    ASSERT_EQ_INT(want.g, e->foreground().g);
    ASSERT_EQ_INT(want.b, e->foreground().b);
    e->palettePick(999);
    ASSERT_EQ_INT(2, e->paletteSelected());
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

TEST(brush_size_does_not_change_shapes)
{
    Editor *e = make_editor();
    e->options().brush_size = 4;
    e->selectTool(ToolKind::Line);
    e->press(0, 0);
    e->drag(3, 0);
    e->release();
    ASSERT_EQ_INT(255, alpha(e, 3, 0));
    ASSERT_EQ_INT(0, alpha(e, 0, 1));
    delete e;
}

TEST(duplicate_layer_selects_the_copy)
{
    Editor *e = make_editor();
    dot(e, 2, 2);
    e->addLayer();
    e->duplicateLayer(0);
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    ASSERT_EQ_INT(1, e->activeLayer());
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    ASSERT_TRUE(e->dirty());
    e->undo();
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    ASSERT_EQ_INT(1, e->activeLayer());
    e->redo();
    ASSERT_EQ_INT(3, e->sprite().layerCount());
    delete e;
}

TEST(merge_layer_down_selects_the_lower_layer)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    e->addLayer();
    dot(e, 3, 3);
    e->mergeLayerDown(1);
    ASSERT_EQ_INT(1, e->sprite().layerCount());
    ASSERT_EQ_INT(0, e->activeLayer());
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    e->undo();
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    ASSERT_EQ_INT(0, alpha(e, 3, 3)); /* active layer 0 again holds only its own pixel */
    e->redo();
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    delete e;
}

TEST(merge_down_follows_a_higher_active_layer)
{
    Editor *e = make_editor();
    e->addLayer();
    e->addLayer();
    ASSERT_EQ_INT(2, e->activeLayer());
    e->mergeLayerDown(1);
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    ASSERT_EQ_INT(1, e->activeLayer());
    delete e;
}

TEST(duplicate_and_merge_are_held_off_while_dragging_or_invalid)
{
    Editor *e = make_editor();
    e->addLayer();
    e->mergeLayerDown(0); /* nothing below */
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    e->selectTool(ToolKind::Pencil);
    e->press(0, 0);
    e->duplicateLayer(0);
    e->mergeLayerDown(1);
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    e->release();
    e->setLayerVisible(1, 0);
    e->mergeLayerDown(1); /* hidden layers do not merge */
    ASSERT_EQ_INT(2, e->sprite().layerCount());
    delete e;
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("test", 0, 0, 64, 64, 0);
    renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : nullptr;
    if (!renderer) {
        printf("SDL renderer: %s\n", SDL_GetError());
        return 1;
    }
    RUN(select_click_on_empty_canvas_selects_one_cel);
    RUN(select_click_outside_a_selection_only_dismisses_it);
    RUN(mirrored_shape_and_bucket_are_symmetric);
    RUN(layer_add_remove_move_undo_redo);
    RUN(visibility_toggle_is_undoable);
    RUN(live_name_and_opacity_edits_commit_as_one_step);
    RUN(unchanged_live_edit_records_nothing);
    RUN(layer_edits_are_held_off_while_dragging);
    RUN(tools_ignore_a_hidden_layer);
    RUN(swap_colors_exchanges_foreground_and_background);
    RUN(temp_pick_switches_to_the_picker_and_back);
    RUN(temp_pick_keeps_the_picker_when_it_is_already_active);
    RUN(selecting_a_tool_ends_a_temp_pick);
    RUN(tool_changes_are_held_off_while_dragging);
    RUN(copy_and_paste_at_the_hovered_cel);
    RUN(paste_without_hover_uses_the_selection_corner);
    RUN(paste_without_a_clipboard_does_nothing);
    RUN(cut_clears_the_selection_and_can_be_pasted_elsewhere);
    RUN(paste_and_cut_ignore_a_hidden_layer);
    RUN(flip_selection_mirrors_only_inside_the_selection);
    RUN(rotate_selection_turns_it_around_its_center);
    RUN(rotate_selection_near_the_edge_keeps_what_fits);
    RUN(selection_transforms_are_held_off_while_dragging);
    RUN(drawing_records_recent_colors);
    RUN(eraser_and_picker_do_not_record_colors);
    RUN(set_color_changes_the_foreground);
    RUN(palette_starts_with_default_colors);
    RUN(palette_add_uses_the_current_color_once);
    RUN(palette_remove_keeps_the_selection_on_the_same_color);
    RUN(pick_palette_color_sets_the_foreground);
    RUN(pencil_uses_the_brush_size_and_marks_it_dirty);
    RUN(round_brush_and_eraser_follow_the_options);
    RUN(brush_size_does_not_change_shapes);
    RUN(duplicate_layer_selects_the_copy);
    RUN(merge_layer_down_selects_the_lower_layer);
    RUN(merge_down_follows_a_higher_active_layer);
    RUN(duplicate_and_merge_are_held_off_while_dragging_or_invalid);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return TEST_SUMMARY();
}
