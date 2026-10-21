#include "editor/editor.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "editor/tools.h"
#include "tinysprite/layer_ops.h"
#include "tinysprite/util.h"

static uint8_t to_byte(float value)
{
    return (uint8_t)clamp_int((int)roundf(value * 255.0f), 0, 255);
}

static void set_floats(float out[4], Rgba color)
{
    out[0] = (float)color.r / 255.0f;
    out[1] = (float)color.g / 255.0f;
    out[2] = (float)color.b / 255.0f;
    out[3] = (float)color.a / 255.0f;
}

/* A starting palette so the panel is useful before the first color is added. */
static const Rgba DEFAULT_PALETTE[] = {
    {0, 0, 0, 255},      {255, 255, 255, 255}, {127, 127, 127, 255}, {195, 195, 195, 255},
    {136, 0, 21, 255},   {237, 28, 36, 255},   {255, 127, 39, 255},  {255, 242, 0, 255},
    {34, 177, 76, 255},  {0, 162, 232, 255},   {63, 72, 204, 255},   {163, 73, 164, 255},
    {185, 122, 87, 255}, {255, 174, 201, 255}, {255, 201, 14, 255},  {181, 230, 29, 255},
};

Editor *editor_create(SDL_Renderer *renderer, int width, int height)
{
    Editor *editor = calloc(1, sizeof(*editor));
    if (!editor) {
        return NULL;
    }
    editor->sprite = sprite_create(width, height);
    editor->history = history_create();
    editor->pixels = calloc((size_t)width * (size_t)height, sizeof(*editor->pixels));
    editor->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                        SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!editor->sprite || !editor->history || !editor->pixels || !editor->texture) {
        editor_destroy(editor);
        return NULL;
    }
    SDL_SetTextureBlendMode(editor->texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(editor->texture, SDL_ScaleModeNearest);

    for (size_t i = 0; i < ARRAY_LEN(DEFAULT_PALETTE); i++) {
        (void)palette_add(sprite_palette(editor->sprite), DEFAULT_PALETTE[i]);
    }
    editor->palette_selected = -1;
    editor->brush_size = 1;
    editor->tool = TOOL_PENCIL;
    set_floats(editor->color, (Rgba){0, 0, 0, 255});
    set_floats(editor->bg_color, (Rgba){255, 255, 255, 255});
    editor->show_grid = true;
    editor->view = viewport_default();
    editor->needs_fit = true;
    editor_mark_all_dirty(editor);
    return editor;
}

void editor_destroy(Editor *editor)
{
    if (!editor) {
        return;
    }
    tools_cancel(editor);
    if (editor->texture) {
        SDL_DestroyTexture(editor->texture);
    }
    clip_destroy(editor->clipboard);
    free(editor->pixels);
    history_destroy(editor->history);
    sprite_destroy(editor->sprite);
    free(editor);
}

Rgba editor_color(const Editor *editor)
{
    return (Rgba){to_byte(editor->color[0]), to_byte(editor->color[1]), to_byte(editor->color[2]),
                  to_byte(editor->color[3])};
}

void editor_set_color(Editor *editor, Rgba color)
{
    set_floats(editor->color, color);
}

int editor_palette_add(Editor *editor)
{
    Palette *palette = sprite_palette(editor->sprite);
    Rgba color = editor_color(editor);
    int index = palette_find(palette, color);
    if (index < 0) {
        index = palette_add(palette, color);
    }
    if (index >= 0) {
        editor->palette_selected = index;
    }
    return index;
}

void editor_palette_pick(Editor *editor, int index)
{
    Palette *palette = sprite_palette(editor->sprite);
    if (index < 0 || index >= palette_count(palette)) {
        return;
    }
    editor->palette_selected = index;
    editor_set_color(editor, palette_get(palette, index));
}

void editor_palette_remove(Editor *editor, int index)
{
    if (palette_remove(sprite_palette(editor->sprite), index) != 0) {
        return;
    }
    if (editor->palette_selected == index) {
        editor->palette_selected = -1;
    } else if (editor->palette_selected > index) {
        editor->palette_selected--;
    }
}

void editor_swap_colors(Editor *editor)
{
    for (int i = 0; i < 4; i++) {
        float t = editor->color[i];
        editor->color[i] = editor->bg_color[i];
        editor->bg_color[i] = t;
    }
}

void editor_select_tool(Editor *editor, ToolKind tool)
{
    if (editor_stroking(editor) || (unsigned)tool >= TOOL_COUNT) {
        return;
    }
    editor->temp_pick = false;
    editor->tool = tool;
}

void editor_temp_pick(Editor *editor, bool on)
{
    if (editor_stroking(editor)) {
        return;
    }
    if (on && !editor->temp_pick && editor->tool != TOOL_EYEDROPPER) {
        editor->pick_return = editor->tool;
        editor->tool = TOOL_EYEDROPPER;
        editor->temp_pick = true;
    } else if (!on && editor->temp_pick) {
        editor->tool = editor->pick_return;
        editor->temp_pick = false;
    }
}

static Rect sprite_bounds(const Editor *editor)
{
    return (Rect){0, 0, sprite_width(editor->sprite), sprite_height(editor->sprite)};
}

void editor_mark_dirty(Editor *editor, Rect area)
{
    editor->dirty_rect = editor->dirty ? rect_union(editor->dirty_rect, area) : area;
    editor->dirty = true;
}

void editor_mark_all_dirty(Editor *editor)
{
    editor->dirty_rect = sprite_bounds(editor);
    editor->dirty = true;
}

void editor_update_texture(Editor *editor)
{
    if (!editor->dirty) {
        return;
    }
    editor->dirty = false;
    int width = sprite_width(editor->sprite);
    Rect r = rect_clip(editor->dirty_rect, width, sprite_height(editor->sprite));
    if (rect_is_empty(r)) {
        return;
    }
    composite_sprite(editor->sprite, editor->frame, r, editor->pixels);
    SDL_Rect rect = {r.x, r.y, r.w, r.h};
    const Rgba *origin = &editor->pixels[(size_t)r.y * (size_t)width + (size_t)r.x];
    SDL_UpdateTexture(editor->texture, &rect, origin, width * (int)sizeof(Rgba));
}

void editor_zoom_step(Editor *editor, int direction)
{
    float zoom = viewport_next_zoom(editor->view.zoom, direction);
    viewport_zoom_at(&editor->view, zoom, editor->canvas_w / 2.0f, editor->canvas_h / 2.0f);
    editor->auto_fit = false;
}

/* Undoing / redoing a layer operation can change the layer count under the active index. */
static void after_history_step(Editor *editor)
{
    editor->active_layer = min_int(editor->active_layer, sprite_layer_count(editor->sprite) - 1);
    editor_mark_all_dirty(editor);
}

void editor_undo(Editor *editor)
{
    if (history_undo(editor->history) == 0) {
        after_history_step(editor);
    }
}

void editor_redo(Editor *editor)
{
    if (history_redo(editor->history) == 0) {
        after_history_step(editor);
    }
}

bool editor_stroking(const Editor *editor)
{
    return editor->drag != DRAG_NONE;
}

bool editor_hover_in_sprite(const Editor *editor)
{
    return editor->hovering && editor->hover_x >= 0 && editor->hover_y >= 0 &&
           editor->hover_x < sprite_width(editor->sprite) &&
           editor->hover_y < sprite_height(editor->sprite);
}

Cel *editor_active_cel(Editor *editor)
{
    Layer *layer = sprite_layer(editor->sprite, editor->active_layer);
    return layer ? layer_cel(layer, editor->frame) : NULL;
}

Cel *editor_editable_cel(Editor *editor)
{
    Layer *layer = sprite_layer(editor->sprite, editor->active_layer);
    return editor_stroking(editor) || !layer || !layer_visible(layer) ? NULL
                                                                      : editor_active_cel(editor);
}

void editor_add_layer(Editor *editor)
{
    if (editor_stroking(editor)) {
        return;
    }
    char name[LAYER_NAME_MAX + 1];
    snprintf(name, sizeof(name), "Layer %d", sprite_layer_count(editor->sprite) + 1);
    if (layer_op_add(editor->history, editor->sprite, name)) {
        editor->active_layer = sprite_layer_count(editor->sprite) - 1;
    }
}

void editor_remove_layer(Editor *editor, int index)
{
    if (editor_stroking(editor) || layer_op_remove(editor->history, editor->sprite, index) != 0) {
        return;
    }
    if (editor->active_layer > index) {
        editor->active_layer--;
    }
    after_history_step(editor);
}

void editor_move_layer(Editor *editor, int from, int to)
{
    if (editor_stroking(editor) || layer_op_move(editor->history, editor->sprite, from, to) != 0) {
        return;
    }
    /* The active layer keeps pointing at the same layer. */
    int active = editor->active_layer;
    if (active == from) {
        editor->active_layer = to;
    } else if (from < active && active <= to) {
        editor->active_layer = active - 1;
    } else if (to <= active && active < from) {
        editor->active_layer = active + 1;
    }
    editor_mark_all_dirty(editor);
}

void editor_set_layer_visible(Editor *editor, int index, bool visible)
{
    Layer *layer = sprite_layer(editor->sprite, index);
    if (!editor_stroking(editor) && layer &&
        layer_op_set_visible(editor->history, layer, visible) == 0) {
        editor_mark_all_dirty(editor);
    }
}

void editor_duplicate_layer(Editor *editor, int index)
{
    if (!editor_stroking(editor) && layer_op_duplicate(editor->history, editor->sprite, index)) {
        editor->active_layer = index + 1;
        editor_mark_all_dirty(editor);
    }
}

void editor_merge_layer_down(Editor *editor, int index)
{
    if (editor_stroking(editor) ||
        layer_op_merge_down(editor->history, editor->sprite, index) != 0) {
        return;
    }
    /* The merged layer is gone: its selection moves down, layers above it shift down by one. */
    if (editor->active_layer == index) {
        editor->active_layer = index - 1;
    } else if (editor->active_layer > index) {
        editor->active_layer--;
    }
    editor_mark_all_dirty(editor);
}

void editor_commit_layer_name(Editor *editor, int index, const char *before_name)
{
    Layer *layer = sprite_layer(editor->sprite, index);
    if (!editor_stroking(editor) && layer) {
        (void)layer_op_record_name(editor->history, layer, before_name);
    }
}

void editor_commit_layer_opacity(Editor *editor, int index, uint8_t before_opacity)
{
    Layer *layer = sprite_layer(editor->sprite, index);
    if (!editor_stroking(editor) && layer) {
        (void)layer_op_record_opacity(editor->history, layer, before_opacity);
        editor_mark_all_dirty(editor);
    }
}

void editor_select_all(Editor *editor)
{
    if (editor_stroking(editor)) {
        return;
    }
    editor->has_selection = true;
    editor->selection = sprite_bounds(editor);
}

void editor_clear_selection(Editor *editor)
{
    if (!editor_stroking(editor)) {
        editor->has_selection = false;
    }
}

void editor_delete_selection(Editor *editor)
{
    Cel *cel = editor_editable_cel(editor);
    if (!cel || !editor->has_selection) {
        return;
    }
    Rect r = editor->selection;
    ShapeStyle erase = {SHAPE_RECT, true, RGBA_TRANSPARENT, SYMMETRY_NONE};
    if (shape_stroke(editor->history, cel, erase, (ToolPoint){r.x, r.y},
                     (ToolPoint){r.x + r.w - 1, r.y + r.h - 1}) == 0) {
        editor_mark_dirty(editor, r);
    }
}

bool editor_copy_selection(Editor *editor)
{
    Cel *cel = editor_active_cel(editor);
    if (editor_stroking(editor) || !editor->has_selection || !cel) {
        return false;
    }
    Clip *clip = clip_copy(cel, editor->selection);
    if (!clip) {
        return false;
    }
    clip_destroy(editor->clipboard);
    editor->clipboard = clip;
    return true;
}

bool editor_cut_selection(Editor *editor)
{
    if (!editor_editable_cel(editor) || !editor_copy_selection(editor)) {
        return false;
    }
    editor_delete_selection(editor);
    return true;
}

void editor_paste(Editor *editor)
{
    Cel *cel = editor_editable_cel(editor);
    if (!cel || !editor->clipboard) {
        return;
    }
    int x = 0;
    int y = 0;
    if (editor_hover_in_sprite(editor)) {
        x = editor->hover_x;
        y = editor->hover_y;
    } else if (editor->has_selection) {
        x = editor->selection.x;
        y = editor->selection.y;
    }
    if (clip_paste(editor->history, cel, editor->clipboard, x, y) != 0) {
        return;
    }
    Rect pasted = {x, y, clip_width(editor->clipboard), clip_height(editor->clipboard)};
    editor_mark_dirty(editor, pasted);
    editor_select_tool(editor, TOOL_SELECT);
    editor_set_selection_clipped(editor, pasted);
}

void editor_set_selection_clipped(Editor *editor, Rect r)
{
    Rect clipped = rect_clip(r, sprite_width(editor->sprite), sprite_height(editor->sprite));
    editor->has_selection = !rect_is_empty(clipped);
    if (editor->has_selection) {
        editor->selection = clipped;
    }
}

void editor_flip_selection(Editor *editor, bool horizontal)
{
    Cel *cel = editor_editable_cel(editor);
    if (!cel || !editor->has_selection) {
        return;
    }
    Rect r = editor->selection;
    Clip *clip = clip_copy(cel, r);
    if (!clip) {
        return;
    }
    if (horizontal) {
        clip_flip_horizontal(clip);
    } else {
        clip_flip_vertical(clip);
    }
    if (clip_replace(editor->history, cel, r, clip, r.x, r.y) == 0) {
        editor_mark_dirty(editor, r);
    }
    clip_destroy(clip);
}

void editor_rotate_selection(Editor *editor, bool clockwise)
{
    Cel *cel = editor_editable_cel(editor);
    if (!cel || !editor->has_selection) {
        return;
    }
    Rect r = editor->selection;
    Clip *clip = clip_copy(cel, r);
    if (!clip || clip_rotate(clip, clockwise) != 0) {
        clip_destroy(clip);
        return;
    }
    /* The turned rectangle shares the old one's center. */
    Rect turned = {r.x + (r.w - r.h) / 2, r.y + (r.h - r.w) / 2, r.h, r.w};
    if (clip_replace(editor->history, cel, r, clip, turned.x, turned.y) == 0) {
        editor_mark_dirty(editor, r);
        editor_mark_dirty(editor, turned);
        editor_set_selection_clipped(editor, turned);
    }
    clip_destroy(clip);
}
