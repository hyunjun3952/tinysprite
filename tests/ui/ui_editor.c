/* Headless tests for the UI's editor state and tool handlers (no ImGui frames, SDL dummy video).
 * Built only with -DTINYSPRITE_BUILD_UI=ON. */
#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "minitest.h"
#include "tools.h"

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
    e->mirror_x = 1;
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
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return TEST_SUMMARY();
}
