#include "editor.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "tinysprite/layer_ops.h"

static uint8_t to_byte(float value)
{
    float scaled = roundf(value * 255.0f);
    if (scaled < 0.0f) {
        return 0;
    }
    if (scaled > 255.0f) {
        return 255;
    }
    return (uint8_t)scaled;
}

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

    editor->tool = TOOL_PENCIL;
    editor->color[0] = 0.0f;
    editor->color[1] = 0.0f;
    editor->color[2] = 0.0f;
    editor->color[3] = 1.0f;
    editor->show_grid = 1;
    editor->view = viewport_default();
    editor->needs_fit = 1;
    editor_mark_all_dirty(editor);
    return editor;
}

void editor_destroy(Editor *editor)
{
    if (!editor) {
        return;
    }
    pencil_stroke_cancel(editor->stroke);
    shape_stroke_cancel(editor->shape);
    move_stroke_cancel(editor->move);
    if (editor->texture) {
        SDL_DestroyTexture(editor->texture);
    }
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

void editor_mark_dirty(Editor *editor, int x0, int y0, int x1, int y1)
{
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    if (editor->dirty) {
        CompositeRect *r = &editor->dirty_rect;
        int right = r->x + r->w - 1;
        int bottom = r->y + r->h - 1;
        x0 = x0 < r->x ? x0 : r->x;
        y0 = y0 < r->y ? y0 : r->y;
        x1 = x1 > right ? x1 : right;
        y1 = y1 > bottom ? y1 : bottom;
    }
    editor->dirty_rect = (CompositeRect){x0, y0, x1 - x0 + 1, y1 - y0 + 1};
    editor->dirty = 1;
}

void editor_mark_all_dirty(Editor *editor)
{
    editor->dirty_rect =
        (CompositeRect){0, 0, sprite_width(editor->sprite), sprite_height(editor->sprite)};
    editor->dirty = 1;
}

void editor_update_texture(Editor *editor)
{
    if (!editor->dirty) {
        return;
    }
    int width = sprite_width(editor->sprite);
    int height = sprite_height(editor->sprite);
    CompositeRect r = editor->dirty_rect;
    int x1 = r.x + r.w > width ? width : r.x + r.w;
    int y1 = r.y + r.h > height ? height : r.y + r.h;
    r.x = r.x < 0 ? 0 : r.x;
    r.y = r.y < 0 ? 0 : r.y;
    editor->dirty = 0;
    if (r.x >= x1 || r.y >= y1) {
        return;
    }
    r.w = x1 - r.x;
    r.h = y1 - r.y;

    composite_sprite(editor->sprite, editor->frame, r, editor->pixels);
    SDL_Rect rect = {r.x, r.y, r.w, r.h};
    const Rgba *origin = &editor->pixels[(size_t)r.y * (size_t)width + (size_t)r.x];
    SDL_UpdateTexture(editor->texture, &rect, origin, width * (int)sizeof(Rgba));
}

void editor_zoom_step(Editor *editor, int direction)
{
    float zoom = viewport_next_zoom(editor->view.zoom, direction);
    viewport_zoom_at(&editor->view, zoom, editor->canvas_w / 2.0f, editor->canvas_h / 2.0f);
}

/* Undoing / redoing a layer operation can change the layer count under the active index. */
static void after_history_step(Editor *editor)
{
    int last = sprite_layer_count(editor->sprite) - 1;
    if (editor->active_layer > last) {
        editor->active_layer = last;
    }
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

int editor_stroking(const Editor *editor)
{
    return editor->dragging;
}

Cel *editor_active_cel(Editor *editor)
{
    Layer *layer = sprite_layer(editor->sprite, editor->active_layer);
    return layer ? layer_cel(layer, editor->frame) : NULL;
}

void editor_add_layer(Editor *editor)
{
    if (editor->dragging) {
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
    if (editor->dragging || layer_op_remove(editor->history, editor->sprite, index) != 0) {
        return;
    }
    if (editor->active_layer > index) {
        editor->active_layer--;
    }
    after_history_step(editor);
}

void editor_move_layer(Editor *editor, int from, int to)
{
    if (editor->dragging || layer_op_move(editor->history, editor->sprite, from, to) != 0) {
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

void editor_set_layer_visible(Editor *editor, int index, int visible)
{
    Layer *layer = sprite_layer(editor->sprite, index);
    if (!editor->dragging && layer && layer_op_set_visible(editor->history, layer, visible) == 0) {
        editor_mark_all_dirty(editor);
    }
}

/* The live edit already changed the layer: put the old value back so the op records old -> new. */
void editor_commit_layer_name(Editor *editor, int index, const char *before_name)
{
    Layer *layer = sprite_layer(editor->sprite, index);
    if (editor->dragging || !layer) {
        return;
    }
    char edited[LAYER_NAME_MAX + 1];
    snprintf(edited, sizeof(edited), "%s", layer_name(layer));
    layer_set_name(layer, before_name);
    if (layer_op_set_name(editor->history, layer, edited) != 0) {
        layer_set_name(layer, edited);
    }
}

void editor_commit_layer_opacity(Editor *editor, int index, uint8_t before_opacity)
{
    Layer *layer = sprite_layer(editor->sprite, index);
    if (editor->dragging || !layer) {
        return;
    }
    uint8_t edited = layer_opacity(layer);
    layer_set_opacity(layer, before_opacity);
    if (layer_op_set_opacity(editor->history, layer, edited) != 0) {
        layer_set_opacity(layer, edited);
    }
    editor_mark_all_dirty(editor);
}

void editor_select_all(Editor *editor)
{
    if (editor->dragging) {
        return;
    }
    editor->has_selection = 1;
    editor->selection =
        (CompositeRect){0, 0, sprite_width(editor->sprite), sprite_height(editor->sprite)};
}

void editor_clear_selection(Editor *editor)
{
    if (!editor->dragging) {
        editor->has_selection = 0;
    }
}

void editor_delete_selection(Editor *editor)
{
    Cel *cel = editor_active_cel(editor);
    Layer *layer = sprite_layer(editor->sprite, editor->active_layer);
    if (editor->dragging || !editor->has_selection || !cel || !layer_visible(layer)) {
        return;
    }
    CompositeRect r = editor->selection;
    if (shape_stroke(editor->history, cel, SHAPE_RECT, 1, (Rgba){0, 0, 0, 0}, r.x, r.y,
                     r.x + r.w - 1, r.y + r.h - 1) == 0) {
        editor_mark_dirty(editor, r.x, r.y, r.x + r.w - 1, r.y + r.h - 1);
    }
}
