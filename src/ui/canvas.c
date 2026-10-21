#include "canvas.h"

#include <stdint.h>
#include <stdio.h>

#include "tools.h"

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

    if (!editor->dragging && hovered && igIsMouseClicked_Bool(ImGuiMouseButton_Left, false)) {
        tools_press(editor, cx, cy);
    } else if (editor->dragging) {
        if (igIsMouseDown_Nil(ImGuiMouseButton_Left)) {
            tools_drag(editor, cx, cy);
        } else {
            tools_release(editor);
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

static void draw_rect_outline(ImDrawList *draw, ImVec2 p0, ImVec2 p1, ImU32 color)
{
    ImDrawList_AddRect(draw, p0, p1, color, 0.0f, 1.0f, 0);
}

/* Selection outline: a dark rectangle under a light one so it shows on any background. */
static void draw_selection(ImDrawList *draw, const Editor *editor, ImVec2 origin)
{
    if (!editor->has_selection) {
        return;
    }
    const CompositeRect *r = &editor->selection;
    int dx = editor->move ? editor->move_dx : 0;
    int dy = editor->move ? editor->move_dy : 0;
    ImVec2 p0 = {origin.x + viewport_cel_to_screen_x(&editor->view, r->x + dx),
                 origin.y + viewport_cel_to_screen_y(&editor->view, r->y + dy)};
    ImVec2 p1 = {p0.x + (float)r->w * editor->view.zoom, p0.y + (float)r->h * editor->view.zoom};
    draw_rect_outline(draw, (ImVec2){p0.x - 1.0f, p0.y - 1.0f}, (ImVec2){p1.x + 1.0f, p1.y + 1.0f},
                      COL32(0, 0, 0, 220));
    draw_rect_outline(draw, p0, p1, COL32(255, 255, 255, 255));
}

/* Center line(s) the symmetry option mirrors across. */
static void draw_mirror_axes(ImDrawList *draw, const Editor *editor, ScreenRect sprite)
{
    ImU32 color = COL32(0, 200, 255, 200);
    if (editor->mirror_x) {
        float x = (sprite.x0 + sprite.x1) / 2.0f;
        ImDrawList_AddLine(draw, (ImVec2){x, sprite.y0}, (ImVec2){x, sprite.y1}, color, 1.0f, 0);
    }
    if (editor->mirror_y) {
        float y = (sprite.y0 + sprite.y1) / 2.0f;
        ImDrawList_AddLine(draw, (ImVec2){sprite.x0, y}, (ImVec2){sprite.x1, y}, color, 1.0f, 0);
    }
}

/* Zoom and hovered cel in the canvas corner. Drawn here, after the canvas has applied this frame's
 * zoom / pan / hover, so the numbers never lag the image. */
static void draw_status(ImDrawList *draw, const Editor *editor, ImVec2 origin, ImVec2 avail)
{
    char text[64];
    int len = snprintf(text, sizeof(text), "Zoom: %gx", (double)editor->view.zoom);
    if (editor->hovering && editor->hover_x >= 0 && editor->hover_y >= 0 &&
        editor->hover_x < sprite_width(editor->sprite) &&
        editor->hover_y < sprite_height(editor->sprite)) {
        snprintf(text + len, sizeof(text) - (size_t)len, "   Cel: %d, %d", editor->hover_x,
                 editor->hover_y);
    }
    ImVec2_c measured = igCalcTextSize(text, NULL, false, -1.0f);
    ImVec2 size = {measured.x, measured.y};
    ImVec2 p0 = {origin.x + 6.0f, origin.y + avail.y - size.y - 10.0f};
    ImDrawList_AddRectFilled(draw, (ImVec2){p0.x - 4.0f, p0.y - 2.0f},
                             (ImVec2){p0.x + size.x + 4.0f, p0.y + size.y + 2.0f},
                             COL32(0, 0, 0, 160), 0.0f, 0);
    ImDrawList_AddText_Vec2(draw, p0, COL32(255, 255, 255, 255), text, NULL);
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
    editor->canvas_w = avail.x;
    editor->canvas_h = avail.y;
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
    draw_mirror_axes(draw, editor, sprite);
    draw_selection(draw, editor, origin);
    draw_hover_outline(draw, editor, origin);
    draw_status(draw, editor, origin, avail);
    ImDrawList_PopClipRect(draw);

    igEnd();
}
