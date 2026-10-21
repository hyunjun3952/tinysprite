/* Headless tests for the UI's editor state and tool handlers (no ImGui frames, SDL dummy video).
 * Built only with -DTINYSPRITE_BUILD_UI=ON. */
#include <stdlib.h>
#include <string.h>

#include "editor/editor.h"
#include "editor/tools.h"
#include "minitest.h"

static SDL_Window *window;
static SDL_Renderer *renderer;

static Editor *make_editor(void)
{
    Editor *e = editor_create(renderer, 8, 6);
    e->color[0] = 1.0f;
    e->color[3] = 1.0f; /* opaque red */
    return e;
}

static int alpha(Editor *e, int x, int y)
{
    return cel_get(editor_active_cel(e), x, y).a;
}

static void click(Editor *e, int x, int y)
{
    tools_press(e, x, y);
    tools_release(e);
}

TEST(select_click_on_empty_canvas_selects_one_cel)
{
    Editor *e = make_editor();
    e->tool = TOOL_SELECT;
    click(e, 3, 2);
    ASSERT_TRUE(e->has_selection);
    ASSERT_EQ_INT(1, e->selection.w);
    ASSERT_EQ_INT(1, e->selection.h);
    ASSERT_EQ_INT(3, e->selection.x);
    ASSERT_EQ_INT(2, e->selection.y);
    editor_destroy(e);
}

TEST(select_click_outside_a_selection_only_dismisses_it)
{
    Editor *e = make_editor();
    e->tool = TOOL_SELECT;
    tools_press(e, 1, 1);
    tools_drag(e, 3, 3);
    tools_release(e);
    ASSERT_EQ_INT(3, e->selection.w);
    click(e, 6, 5);
    ASSERT_TRUE(!e->has_selection);
    click(e, 6, 5);
    ASSERT_TRUE(e->has_selection);
    editor_destroy(e);
}

TEST(mirrored_shape_and_bucket_are_symmetric)
{
    Editor *e = make_editor();
    e->symmetry.mirror_x = true;
    e->tool = TOOL_LINE;
    tools_press(e, 0, 0);
    tools_drag(e, 2, 0);
    tools_release(e);
    ASSERT_EQ_INT(255, alpha(e, 7, 0));
    ASSERT_EQ_INT(255, alpha(e, 5, 0));
    ASSERT_EQ_INT(0, alpha(e, 4, 0));

    editor_undo(e);
    e->tool = TOOL_RECT;
    tools_press(e, 0, 0);
    tools_drag(e, 7, 0);
    tools_release(e);
    e->tool = TOOL_BUCKET;
    click(e, 0, 3);
    ASSERT_EQ_INT(255, alpha(e, 0, 5));
    ASSERT_EQ_INT(255, alpha(e, 7, 5));
    editor_destroy(e);
}

TEST(layer_add_remove_move_undo_redo)
{
    Editor *e = make_editor();
    editor_add_layer(e);
    editor_add_layer(e);
    ASSERT_EQ_INT(3, sprite_layer_count(e->sprite));
    ASSERT_EQ_INT(2, e->active_layer);
    e->tool = TOOL_PENCIL;
    click(e, 1, 1);
    editor_remove_layer(e, 2);
    ASSERT_EQ_INT(2, sprite_layer_count(e->sprite));
    ASSERT_EQ_INT(1, e->active_layer);
    editor_undo(e);
    ASSERT_EQ_INT(3, sprite_layer_count(e->sprite));
    editor_undo(e); /* the pixel on the restored layer */
    editor_undo(e); /* second add */
    ASSERT_EQ_INT(2, sprite_layer_count(e->sprite));
    ASSERT_EQ_INT(1, e->active_layer);
    editor_undo(e);
    ASSERT_EQ_INT(1, sprite_layer_count(e->sprite));
    ASSERT_EQ_INT(0, e->active_layer);
    editor_redo(e);
    editor_redo(e);
    ASSERT_EQ_INT(3, sprite_layer_count(e->sprite));
    editor_destroy(e);
}

TEST(visibility_toggle_is_undoable)
{
    Editor *e = make_editor();
    editor_set_layer_visible(e, 0, 0);
    ASSERT_TRUE(!layer_visible(sprite_layer(e->sprite, 0)));
    editor_undo(e);
    ASSERT_TRUE(layer_visible(sprite_layer(e->sprite, 0)));
    editor_destroy(e);
}

TEST(live_name_and_opacity_edits_commit_as_one_step)
{
    Editor *e = make_editor();
    Layer *layer = sprite_layer(e->sprite, 0);
    layer_set_name(layer, "B");
    layer_set_name(layer, "Bo");
    layer_set_name(layer, "Body");
    editor_commit_layer_name(e, 0, "Layer 1");
    layer_set_opacity(layer, 200);
    layer_set_opacity(layer, 90);
    editor_commit_layer_opacity(e, 0, 255);
    ASSERT_TRUE(strcmp(layer_name(layer), "Body") == 0);
    ASSERT_EQ_INT(90, layer_opacity(layer));
    editor_undo(e);
    ASSERT_EQ_INT(255, layer_opacity(layer));
    editor_undo(e);
    ASSERT_TRUE(strcmp(layer_name(layer), "Layer 1") == 0);
    ASSERT_TRUE(!history_can_undo(e->history));
    editor_redo(e);
    ASSERT_TRUE(strcmp(layer_name(layer), "Body") == 0);
    editor_destroy(e);
}

TEST(unchanged_live_edit_records_nothing)
{
    Editor *e = make_editor();
    editor_commit_layer_name(e, 0, "Layer 1");
    editor_commit_layer_opacity(e, 0, 255);
    ASSERT_TRUE(!history_can_undo(e->history));
    editor_destroy(e);
}

TEST(layer_edits_are_held_off_while_dragging)
{
    Editor *e = make_editor();
    e->tool = TOOL_PENCIL;
    tools_press(e, 0, 0);
    editor_add_layer(e);
    editor_set_layer_visible(e, 0, 0);
    ASSERT_EQ_INT(1, sprite_layer_count(e->sprite));
    ASSERT_TRUE(layer_visible(sprite_layer(e->sprite, 0)));
    tools_release(e);
    editor_destroy(e);
}

TEST(tools_ignore_a_hidden_layer)
{
    Editor *e = make_editor();
    e->tool = TOOL_PENCIL;
    editor_set_layer_visible(e, 0, 0);
    click(e, 2, 2);
    ASSERT_EQ_INT(0, alpha(e, 2, 2));
    editor_destroy(e);
}

TEST(swap_colors_exchanges_foreground_and_background)
{
    Editor *e = make_editor();
    ASSERT_EQ_INT(255, editor_color(e).r);
    ASSERT_EQ_INT(0, editor_color(e).g);
    editor_swap_colors(e);
    ASSERT_EQ_INT(255, editor_color(e).g);
    ASSERT_EQ_INT(255, e->bg_color[0] > 0.5f ? 255 : 0);
    ASSERT_EQ_INT(0, e->bg_color[1] > 0.5f ? 255 : 0);
    editor_swap_colors(e);
    ASSERT_EQ_INT(0, editor_color(e).g);
    editor_destroy(e);
}

TEST(temp_pick_switches_to_the_picker_and_back)
{
    Editor *e = make_editor();
    e->tool = TOOL_RECT;
    editor_temp_pick(e, 1);
    ASSERT_EQ_INT(TOOL_EYEDROPPER, e->tool);
    editor_temp_pick(e, 1); /* held: stays on the picker, remembers the first tool */
    editor_temp_pick(e, 0);
    ASSERT_EQ_INT(TOOL_RECT, e->tool);
    editor_temp_pick(e, 0);
    ASSERT_EQ_INT(TOOL_RECT, e->tool);
    editor_destroy(e);
}

TEST(temp_pick_keeps_the_picker_when_it_is_already_active)
{
    Editor *e = make_editor();
    e->tool = TOOL_EYEDROPPER;
    editor_temp_pick(e, 1);
    editor_temp_pick(e, 0);
    ASSERT_EQ_INT(TOOL_EYEDROPPER, e->tool);
    editor_destroy(e);
}

TEST(selecting_a_tool_ends_a_temp_pick)
{
    Editor *e = make_editor();
    editor_temp_pick(e, 1);
    editor_select_tool(e, TOOL_BUCKET);
    editor_temp_pick(e, 0);
    ASSERT_EQ_INT(TOOL_BUCKET, e->tool);
    editor_destroy(e);
}

TEST(tool_changes_are_held_off_while_dragging)
{
    Editor *e = make_editor();
    e->tool = TOOL_PENCIL;
    tools_press(e, 0, 0);
    editor_select_tool(e, TOOL_BUCKET);
    editor_temp_pick(e, 1);
    ASSERT_EQ_INT(TOOL_PENCIL, e->tool);
    tools_release(e);
    editor_temp_pick(e, 1);
    ASSERT_EQ_INT(TOOL_EYEDROPPER, e->tool);
    editor_destroy(e);
}

/* Draws a red pixel at each of the given cels with the pencil. */
static void dot(Editor *e, int x, int y)
{
    e->tool = TOOL_PENCIL;
    click(e, x, y);
}

static void select_rect(Editor *e, int x0, int y0, int x1, int y1)
{
    e->tool = TOOL_SELECT;
    tools_press(e, x0, y0);
    tools_drag(e, x1, y1);
    tools_release(e);
}

TEST(copy_and_paste_at_the_hovered_cel)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    dot(e, 2, 1);
    select_rect(e, 1, 1, 2, 1);
    ASSERT_TRUE(editor_copy_selection(e));
    e->hovering = 1;
    e->hover_x = 4;
    e->hover_y = 3;
    editor_paste(e);
    ASSERT_EQ_INT(255, alpha(e, 4, 3));
    ASSERT_EQ_INT(255, alpha(e, 5, 3));
    ASSERT_EQ_INT(255, alpha(e, 1, 1)); /* copying leaves the source alone */
    ASSERT_EQ_INT(TOOL_SELECT, e->tool);
    ASSERT_TRUE(e->has_selection);
    ASSERT_EQ_INT(4, e->selection.x);
    ASSERT_EQ_INT(3, e->selection.y);
    ASSERT_EQ_INT(2, e->selection.w);
    editor_undo(e);
    ASSERT_EQ_INT(0, alpha(e, 4, 3));
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    editor_destroy(e);
}

TEST(paste_without_hover_uses_the_selection_corner)
{
    Editor *e = make_editor();
    dot(e, 3, 3);
    select_rect(e, 3, 3, 3, 3);
    editor_copy_selection(e);
    select_rect(e, 5, 4, 6, 5);
    editor_paste(e);
    ASSERT_EQ_INT(255, alpha(e, 5, 4));
    editor_clear_selection(e);
    editor_undo(e);
    editor_paste(e); /* no selection and no hover: top-left of the sprite */
    ASSERT_EQ_INT(255, alpha(e, 0, 0));
    editor_destroy(e);
}

TEST(paste_without_a_clipboard_does_nothing)
{
    Editor *e = make_editor();
    editor_paste(e);
    ASSERT_TRUE(!history_can_undo(e->history));
    ASSERT_TRUE(!editor_copy_selection(e)); /* nothing selected */
    editor_destroy(e);
}

TEST(cut_clears_the_selection_and_can_be_pasted_elsewhere)
{
    Editor *e = make_editor();
    dot(e, 2, 2);
    select_rect(e, 2, 2, 3, 3);
    ASSERT_TRUE(editor_cut_selection(e));
    ASSERT_EQ_INT(0, alpha(e, 2, 2));
    e->hovering = 1;
    e->hover_x = 6;
    e->hover_y = 0;
    editor_paste(e);
    ASSERT_EQ_INT(255, alpha(e, 6, 0));
    editor_undo(e);
    editor_undo(e); /* the cut */
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    editor_destroy(e);
}

TEST(paste_and_cut_ignore_a_hidden_layer)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    select_rect(e, 1, 1, 1, 1);
    editor_copy_selection(e);
    editor_set_layer_visible(e, 0, 0);
    ASSERT_TRUE(!editor_cut_selection(e));
    editor_paste(e);
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    editor_set_layer_visible(e, 0, 1);
    editor_destroy(e);
}

TEST(flip_selection_mirrors_only_inside_the_selection)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    dot(e, 5, 1); /* outside the selection */
    select_rect(e, 1, 1, 3, 2);
    editor_flip_selection(e, 1);
    ASSERT_EQ_INT(0, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 3, 1));
    ASSERT_EQ_INT(255, alpha(e, 5, 1));
    editor_flip_selection(e, 0);
    ASSERT_EQ_INT(255, alpha(e, 3, 2));
    ASSERT_EQ_INT(0, alpha(e, 3, 1));
    editor_undo(e);
    editor_undo(e);
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    ASSERT_EQ_INT(1, e->selection.x);
    editor_destroy(e);
}

TEST(rotate_selection_turns_it_around_its_center)
{
    Editor *e = make_editor();
    dot(e, 1, 2);
    select_rect(e, 1, 2, 3, 2); /* a 3 x 1 strip with the pixel at its left end */
    editor_rotate_selection(e, 1);
    ASSERT_EQ_INT(1, e->selection.w);
    ASSERT_EQ_INT(3, e->selection.h);
    ASSERT_EQ_INT(2, e->selection.x);
    ASSERT_EQ_INT(1, e->selection.y);
    ASSERT_EQ_INT(0, alpha(e, 1, 2));
    ASSERT_EQ_INT(255, alpha(e, 2, 1)); /* clockwise: the left end goes to the top */
    editor_undo(e);
    ASSERT_EQ_INT(255, alpha(e, 1, 2));
    ASSERT_EQ_INT(0, alpha(e, 2, 1));
    editor_destroy(e);
}

TEST(rotate_selection_near_the_edge_keeps_what_fits)
{
    Editor *e = make_editor();
    dot(e, 0, 0);
    dot(e, 1, 0);
    dot(e, 2, 0);
    select_rect(e, 0, 0, 2, 0);
    editor_rotate_selection(e, 1);
    ASSERT_TRUE(e->has_selection);
    ASSERT_EQ_INT(0, e->selection.y);
    ASSERT_EQ_INT(2, e->selection.h); /* the top row of the turned strip fell off the sprite */
    ASSERT_EQ_INT(255, alpha(e, 1, 0));
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    editor_destroy(e);
}

TEST(selection_transforms_are_held_off_while_dragging)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    select_rect(e, 1, 1, 2, 2);
    e->tool = TOOL_PENCIL;
    tools_press(e, 6, 5);
    editor_flip_selection(e, 1);
    editor_rotate_selection(e, 1);
    ASSERT_TRUE(!editor_copy_selection(e));
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    tools_release(e);
    editor_destroy(e);
}

TEST(drawing_records_recent_colors)
{
    Editor *e = make_editor();
    ASSERT_EQ_INT(0, e->recent.count);
    dot(e, 0, 0);
    ASSERT_EQ_INT(1, e->recent.count);
    ASSERT_EQ_INT(255, e->recent.colors[0].r);
    dot(e, 1, 0); /* the same color again */
    ASSERT_EQ_INT(1, e->recent.count);

    editor_set_color(e, (Rgba){0, 0, 255, 255});
    e->tool = TOOL_BUCKET;
    click(e, 5, 5);
    ASSERT_EQ_INT(2, e->recent.count);
    ASSERT_EQ_INT(255, e->recent.colors[0].b);

    editor_set_color(e, (Rgba){0, 255, 0, 255});
    e->tool = TOOL_RECT;
    tools_press(e, 2, 2);
    tools_drag(e, 3, 3);
    tools_release(e);
    ASSERT_EQ_INT(3, e->recent.count);
    editor_destroy(e);
}

TEST(eraser_and_picker_do_not_record_colors)
{
    Editor *e = make_editor();
    e->tool = TOOL_ERASER;
    click(e, 0, 0);
    e->tool = TOOL_EYEDROPPER;
    click(e, 0, 0);
    ASSERT_EQ_INT(0, e->recent.count);
    editor_destroy(e);
}

TEST(set_color_changes_the_foreground)
{
    Editor *e = make_editor();
    editor_set_color(e, (Rgba){0, 128, 255, 51});
    Rgba c = editor_color(e);
    ASSERT_EQ_INT(0, c.r);
    ASSERT_EQ_INT(128, c.g);
    ASSERT_EQ_INT(255, c.b);
    ASSERT_EQ_INT(51, c.a);
    editor_destroy(e);
}

TEST(palette_starts_with_default_colors)
{
    Editor *e = make_editor();
    ASSERT_TRUE(palette_count(sprite_palette(e->sprite)) > 0);
    editor_destroy(e);
}

TEST(palette_add_uses_the_current_color_once)
{
    Editor *e = make_editor();
    Palette *p = sprite_palette(e->sprite);
    int before = palette_count(p);
    editor_set_color(e, (Rgba){12, 34, 56, 255});
    int index = editor_palette_add(e);
    ASSERT_EQ_INT(before, index);
    ASSERT_EQ_INT(before + 1, palette_count(p));
    ASSERT_EQ_INT(index, e->palette_selected);
    ASSERT_EQ_INT(56, palette_get(p, index).b);
    ASSERT_EQ_INT(index, editor_palette_add(e)); /* already there: no duplicate */
    ASSERT_EQ_INT(before + 1, palette_count(p));
    editor_destroy(e);
}

TEST(palette_remove_keeps_the_selection_on_the_same_color)
{
    Editor *e = make_editor();
    Palette *p = sprite_palette(e->sprite);
    int count = palette_count(p);
    e->palette_selected = count - 1;
    Rgba last = palette_get(p, count - 1);
    editor_palette_remove(e, 0);
    ASSERT_EQ_INT(count - 1, palette_count(p));
    ASSERT_EQ_INT(count - 2, e->palette_selected);
    ASSERT_EQ_INT(last.r, palette_get(p, e->palette_selected).r);
    editor_palette_remove(e, e->palette_selected); /* removing the selected swatch deselects */
    ASSERT_TRUE(e->palette_selected < palette_count(p));
    editor_palette_remove(e, 999);
    ASSERT_EQ_INT(count - 2, palette_count(p));
    editor_destroy(e);
}

TEST(pick_palette_color_sets_the_foreground)
{
    Editor *e = make_editor();
    Palette *p = sprite_palette(e->sprite);
    Rgba want = palette_get(p, 2);
    editor_palette_pick(e, 2);
    ASSERT_EQ_INT(2, e->palette_selected);
    ASSERT_EQ_INT(want.g, editor_color(e).g);
    ASSERT_EQ_INT(want.b, editor_color(e).b);
    editor_palette_pick(e, 999);
    ASSERT_EQ_INT(2, e->palette_selected);
    editor_destroy(e);
}

TEST(pencil_uses_the_brush_size_and_marks_it_dirty)
{
    Editor *e = make_editor();
    e->brush_size = 3;
    e->tool = TOOL_PENCIL;
    tools_press(e, 3, 2);
    tools_release(e);
    ASSERT_EQ_INT(255, alpha(e, 2, 1));
    ASSERT_EQ_INT(255, alpha(e, 4, 3));
    ASSERT_EQ_INT(0, alpha(e, 5, 3));
    ASSERT_TRUE(e->dirty);
    ASSERT_TRUE(e->dirty_rect.x <= 2 && e->dirty_rect.y <= 1);
    ASSERT_TRUE(e->dirty_rect.x + e->dirty_rect.w > 4 && e->dirty_rect.y + e->dirty_rect.h > 3);
    editor_undo(e);
    ASSERT_EQ_INT(0, alpha(e, 3, 2));
    editor_destroy(e);
}

TEST(round_brush_and_eraser_follow_the_options)
{
    Editor *e = make_editor();
    e->brush_size = 3;
    e->brush_round = true;
    e->tool = TOOL_PENCIL;
    click(e, 3, 2);
    ASSERT_EQ_INT(0, alpha(e, 2, 1)); /* corner cut off */
    ASSERT_EQ_INT(255, alpha(e, 3, 1));
    e->tool = TOOL_ERASER;
    e->brush_size = 1;
    click(e, 3, 2);
    ASSERT_EQ_INT(0, alpha(e, 3, 2));
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    editor_destroy(e);
}

TEST(brush_size_does_not_change_shapes)
{
    Editor *e = make_editor();
    e->brush_size = 4;
    e->tool = TOOL_LINE;
    tools_press(e, 0, 0);
    tools_drag(e, 3, 0);
    tools_release(e);
    ASSERT_EQ_INT(255, alpha(e, 3, 0));
    ASSERT_EQ_INT(0, alpha(e, 0, 1));
    editor_destroy(e);
}

TEST(duplicate_layer_selects_the_copy)
{
    Editor *e = make_editor();
    dot(e, 2, 2);
    editor_add_layer(e);
    editor_duplicate_layer(e, 0);
    ASSERT_EQ_INT(3, sprite_layer_count(e->sprite));
    ASSERT_EQ_INT(1, e->active_layer);
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    ASSERT_TRUE(e->dirty);
    editor_undo(e);
    ASSERT_EQ_INT(2, sprite_layer_count(e->sprite));
    ASSERT_EQ_INT(1, e->active_layer);
    editor_redo(e);
    ASSERT_EQ_INT(3, sprite_layer_count(e->sprite));
    editor_destroy(e);
}

TEST(merge_layer_down_selects_the_lower_layer)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    editor_add_layer(e);
    dot(e, 3, 3);
    editor_merge_layer_down(e, 1);
    ASSERT_EQ_INT(1, sprite_layer_count(e->sprite));
    ASSERT_EQ_INT(0, e->active_layer);
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    editor_undo(e);
    ASSERT_EQ_INT(2, sprite_layer_count(e->sprite));
    ASSERT_EQ_INT(0, alpha(e, 3, 3)); /* active layer 0 again holds only its own pixel */
    editor_redo(e);
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    editor_destroy(e);
}

TEST(merge_down_follows_a_higher_active_layer)
{
    Editor *e = make_editor();
    editor_add_layer(e);
    editor_add_layer(e);
    ASSERT_EQ_INT(2, e->active_layer);
    editor_merge_layer_down(e, 1);
    ASSERT_EQ_INT(2, sprite_layer_count(e->sprite));
    ASSERT_EQ_INT(1, e->active_layer);
    editor_destroy(e);
}

TEST(duplicate_and_merge_are_held_off_while_dragging_or_invalid)
{
    Editor *e = make_editor();
    editor_add_layer(e);
    editor_merge_layer_down(e, 0); /* nothing below */
    ASSERT_EQ_INT(2, sprite_layer_count(e->sprite));
    e->tool = TOOL_PENCIL;
    tools_press(e, 0, 0);
    editor_duplicate_layer(e, 0);
    editor_merge_layer_down(e, 1);
    ASSERT_EQ_INT(2, sprite_layer_count(e->sprite));
    tools_release(e);
    editor_set_layer_visible(e, 1, 0);
    editor_merge_layer_down(e, 1); /* hidden layers do not merge */
    ASSERT_EQ_INT(2, sprite_layer_count(e->sprite));
    editor_destroy(e);
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("test", 0, 0, 64, 64, 0);
    renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : NULL;
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
