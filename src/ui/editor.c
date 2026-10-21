#include "editor.h"

#include <math.h>
#include <stdlib.h>

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
    if (editor->stroke) {
        pencil_stroke_cancel(editor->stroke);
    }
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

void editor_undo(Editor *editor)
{
    if (history_undo(editor->history) == 0) {
        editor_mark_all_dirty(editor);
    }
}

void editor_redo(Editor *editor)
{
    if (history_redo(editor->history) == 0) {
        editor_mark_all_dirty(editor);
    }
}
