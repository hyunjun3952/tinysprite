#include "canvas.h"

#include <stdint.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#define COL32(r, g, b, a)                                                                          \
    ((ImU32)(((ImU32)(a) << 24) | ((ImU32)(b) << 16) | ((ImU32)(g) << 8) | (ImU32)(r)))

enum {
    CHECKER_TILE = 8,
    GRID_MIN_ZOOM = 4,
    FIT_MARGIN = 16,
};

static float maxf(float a, float b)
{
    return a > b ? a : b;
}

static float minf(float a, float b)
{
    return a < b ? a : b;
}

/* Screen-space rectangle (canvas window origin + view transform) of the whole sprite. */
typedef struct ScreenRect {
    float x0, y0, x1, y1;
} ScreenRect;

static void begin_stroke(Editor *editor, int cx, int cy)
{
    Cel *cel = layer_cel(sprite_layer(editor->sprite, editor->active_layer), editor->frame);
    editor->stroke = pencil_stroke_begin(editor->history, cel, editor_color(editor),
                                         editor->tool == TOOL_ERASER, cx, cy);
    if (editor->stroke) {
        editor->stroke_x = cx;
        editor->stroke_y = cy;
        editor_mark_dirty(editor, cx, cy, cx, cy);
    }
}

static void extend_stroke(Editor *editor, int cx, int cy)
{
    if (cx == editor->stroke_x && cy == editor->stroke_y) {
        return;
    }
    pencil_stroke_extend(editor->stroke, cx, cy);
    editor_mark_dirty(editor, editor->stroke_x, editor->stroke_y, cx, cy);
    editor->stroke_x = cx;
    editor->stroke_y = cy;
}

static void end_stroke(Editor *editor)
{
    pencil_stroke_end(editor->stroke);
    editor->stroke = NULL;
}

static void handle_input(Editor *editor, ImVec2 origin, int hovered, int active)
{
    ImGuiIO *io = igGetIO_Nil();
    ImVec2 mouse = igGetMousePos();
    float local_x = mouse.x - origin.x;
    float local_y = mouse.y - origin.y;
    int cx = viewport_screen_to_cel_x(&editor->view, local_x);
    int cy = viewport_screen_to_cel_y(&editor->view, local_y);

    editor->hovering = hovered;
    editor->hover_x = cx;
    editor->hover_y = cy;

    if (hovered && io->MouseWheel != 0.0f) {
        float zoom = viewport_next_zoom(editor->view.zoom, io->MouseWheel > 0.0f ? 1 : -1);
        viewport_zoom_at(&editor->view, zoom, local_x, local_y);
    }
    if (active && igIsMouseDown_Nil(ImGuiMouseButton_Middle)) {
        editor->view.pan_x += io->MouseDelta.x;
        editor->view.pan_y += io->MouseDelta.y;
    }

    if (!editor->stroke && hovered && igIsMouseClicked_Bool(ImGuiMouseButton_Left, false)) {
        begin_stroke(editor, cx, cy);
    } else if (editor->stroke) {
        if (igIsMouseDown_Nil(ImGuiMouseButton_Left)) {
            extend_stroke(editor, cx, cy);
        } else {
            end_stroke(editor);
        }
    }
}

static void draw_checkerboard(ImDrawList *draw, ScreenRect visible, ScreenRect sprite)
{
    ImDrawList_AddRectFilled(draw, (ImVec2){visible.x0, visible.y0},
                             (ImVec2){visible.x1, visible.y1}, COL32(204, 204, 204, 255), 0.0f, 0);
    int tx0 = (int)((visible.x0 - sprite.x0) / CHECKER_TILE);
    int tx1 = (int)((visible.x1 - sprite.x0) / CHECKER_TILE);
    int ty0 = (int)((visible.y0 - sprite.y0) / CHECKER_TILE);
    int ty1 = (int)((visible.y1 - sprite.y0) / CHECKER_TILE);
    for (int ty = ty0; ty <= ty1; ty++) {
        for (int tx = tx0; tx <= tx1; tx++) {
            if ((tx + ty) % 2 == 0) {
                continue;
            }
            float x0 = maxf(sprite.x0 + (float)(tx * CHECKER_TILE), visible.x0);
            float y0 = maxf(sprite.y0 + (float)(ty * CHECKER_TILE), visible.y0);
            float x1 = minf(sprite.x0 + (float)((tx + 1) * CHECKER_TILE), visible.x1);
            float y1 = minf(sprite.y0 + (float)((ty + 1) * CHECKER_TILE), visible.y1);
            if (x0 < x1 && y0 < y1) {
                ImDrawList_AddRectFilled(draw, (ImVec2){x0, y0}, (ImVec2){x1, y1},
                                         COL32(153, 153, 153, 255), 0.0f, 0);
            }
        }
    }
}

static void draw_image(ImDrawList *draw, SDL_Texture *texture, ScreenRect visible,
                       ScreenRect sprite)
{
    float width = sprite.x1 - sprite.x0;
    float height = sprite.y1 - sprite.y0;
    ImVec2 uv0 = {(visible.x0 - sprite.x0) / width, (visible.y0 - sprite.y0) / height};
    ImVec2 uv1 = {(visible.x1 - sprite.x0) / width, (visible.y1 - sprite.y0) / height};
    ImTextureRef_c ref = {NULL, (ImTextureID)(uintptr_t)texture};
    ImDrawList_AddImage(draw, ref, (ImVec2){visible.x0, visible.y0},
                        (ImVec2){visible.x1, visible.y1}, uv0, uv1, COL32(255, 255, 255, 255));
}

static void draw_grid(ImDrawList *draw, const Editor *editor, ImVec2 origin, ScreenRect visible)
{
    const Viewport *view = &editor->view;
    int first_x = viewport_screen_to_cel_x(view, visible.x0 - origin.x);
    int last_x = viewport_screen_to_cel_x(view, visible.x1 - origin.x);
    int first_y = viewport_screen_to_cel_y(view, visible.y0 - origin.y);
    int last_y = viewport_screen_to_cel_y(view, visible.y1 - origin.y);
    ImU32 color = COL32(0, 0, 0, 60);
    for (int x = first_x; x <= last_x; x++) {
        float sx = origin.x + viewport_cel_to_screen_x(view, x);
        if (sx >= visible.x0 && sx <= visible.x1) {
            ImDrawList_AddLine(draw, (ImVec2){sx, visible.y0}, (ImVec2){sx, visible.y1}, color,
                               1.0f, 0);
        }
    }
    for (int y = first_y; y <= last_y; y++) {
        float sy = origin.y + viewport_cel_to_screen_y(view, y);
        if (sy >= visible.y0 && sy <= visible.y1) {
            ImDrawList_AddLine(draw, (ImVec2){visible.x0, sy}, (ImVec2){visible.x1, sy}, color,
                               1.0f, 0);
        }
    }
}

static void draw_hover_outline(ImDrawList *draw, const Editor *editor, ImVec2 origin)
{
    int w = sprite_width(editor->sprite);
    int h = sprite_height(editor->sprite);
    if (!editor->hovering || editor->hover_x < 0 || editor->hover_y < 0 || editor->hover_x >= w ||
        editor->hover_y >= h) {
        return;
    }
    float x0 = origin.x + viewport_cel_to_screen_x(&editor->view, editor->hover_x);
    float y0 = origin.y + viewport_cel_to_screen_y(&editor->view, editor->hover_y);
    float x1 = x0 + editor->view.zoom;
    float y1 = y0 + editor->view.zoom;
    ImU32 color = COL32(255, 255, 255, 220);
    ImDrawList_AddLine(draw, (ImVec2){x0, y0}, (ImVec2){x1, y0}, color, 1.0f, 0);
    ImDrawList_AddLine(draw, (ImVec2){x1, y0}, (ImVec2){x1, y1}, color, 1.0f, 0);
    ImDrawList_AddLine(draw, (ImVec2){x1, y1}, (ImVec2){x0, y1}, color, 1.0f, 0);
    ImDrawList_AddLine(draw, (ImVec2){x0, y1}, (ImVec2){x0, y0}, color, 1.0f, 0);
}

void canvas_show(Editor *editor, float x, float y, float w, float h)
{
    igSetNextWindowPos((ImVec2){x, y}, ImGuiCond_Always, (ImVec2){0.0f, 0.0f});
    igSetNextWindowSize((ImVec2){w, h}, ImGuiCond_Always);
    igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, (ImVec2){0.0f, 0.0f});
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    igBegin("Canvas", NULL, flags);
    igPopStyleVar(1);

    ImVec2 origin = igGetCursorScreenPos();
    ImVec2 avail = igGetContentRegionAvail();
    int sprite_w = sprite_width(editor->sprite);
    int sprite_h = sprite_height(editor->sprite);
    if (editor->needs_fit && avail.x > 0.0f && avail.y > 0.0f) {
        viewport_fit(&editor->view, sprite_w, sprite_h, maxf(avail.x - FIT_MARGIN, 1.0f),
                     maxf(avail.y - FIT_MARGIN, 1.0f));
        editor->view.pan_x += FIT_MARGIN / 2.0f;
        editor->view.pan_y += FIT_MARGIN / 2.0f;
        editor->needs_fit = 0;
    }

    igInvisibleButton("canvas", avail,
                      ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    handle_input(editor, origin, igIsItemHovered(0), igIsItemActive());
    editor_update_texture(editor);

    ImDrawList *draw = igGetWindowDrawList();
    ImVec2 clip_max = {origin.x + avail.x, origin.y + avail.y};
    ImDrawList_PushClipRect(draw, origin, clip_max, true);
    ImDrawList_AddRectFilled(draw, origin, clip_max, COL32(45, 45, 48, 255), 0.0f, 0);

    ScreenRect sprite = {origin.x + viewport_cel_to_screen_x(&editor->view, 0),
                         origin.y + viewport_cel_to_screen_y(&editor->view, 0), 0.0f, 0.0f};
    sprite.x1 = sprite.x0 + (float)sprite_w * editor->view.zoom;
    sprite.y1 = sprite.y0 + (float)sprite_h * editor->view.zoom;
    ScreenRect visible = {maxf(sprite.x0, origin.x), maxf(sprite.y0, origin.y),
                          minf(sprite.x1, clip_max.x), minf(sprite.y1, clip_max.y)};
    if (visible.x0 < visible.x1 && visible.y0 < visible.y1) {
        draw_checkerboard(draw, visible, sprite);
        draw_image(draw, editor->texture, visible, sprite);
        if (editor->show_grid && editor->view.zoom >= GRID_MIN_ZOOM) {
            draw_grid(draw, editor, origin, visible);
        }
    }
    draw_hover_outline(draw, editor, origin);
    ImDrawList_PopClipRect(draw);

    igEnd();
}
