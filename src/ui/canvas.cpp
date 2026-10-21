#include "canvas.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>

#include "theme.h"

namespace
{

constexpr int CHECKER_TILE = 8;
constexpr float GRID_MIN_ZOOM = 4.0f;
constexpr float FIT_MARGIN = 16.0f;
constexpr double MIDDLE_HOLD_SECONDS = 0.25; /* a middle press this long is the eyedropper */
constexpr float MIDDLE_DRAG_PIXELS = 4.0f;   /* ... unless the mouse moves this far first: pan */

void drawCheckerboard(ImDrawList *draw, float vx0, float vy0, float vx1, float vy1, float sx0,
                      float sy0)
{
    draw->AddRectFilled(ImVec2(vx0, vy0), ImVec2(vx1, vy1), IM_COL32(170, 170, 170, 255));
    int tx0 = static_cast<int>((vx0 - sx0) / CHECKER_TILE);
    int tx1 = static_cast<int>((vx1 - sx0) / CHECKER_TILE);
    int ty0 = static_cast<int>((vy0 - sy0) / CHECKER_TILE);
    int ty1 = static_cast<int>((vy1 - sy0) / CHECKER_TILE);
    for (int ty = ty0; ty <= ty1; ty++) {
        for (int tx = tx0; tx <= tx1; tx++) {
            if ((tx + ty) % 2 == 0) {
                continue;
            }
            float x0 = std::max(sx0 + static_cast<float>(tx * CHECKER_TILE), vx0);
            float y0 = std::max(sy0 + static_cast<float>(ty * CHECKER_TILE), vy0);
            float x1 = std::min(sx0 + static_cast<float>((tx + 1) * CHECKER_TILE), vx1);
            float y1 = std::min(sy0 + static_cast<float>((ty + 1) * CHECKER_TILE), vy1);
            if (x0 < x1 && y0 < y1) {
                draw->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(150, 150, 150, 255));
            }
        }
    }
}

void drawRectOutline(ImDrawList *draw, ImVec2 p0, ImVec2 p1, ImU32 color)
{
    draw->AddRect(p0, p1, color);
}

} // namespace

Canvas::Canvas(Editor &editor) : editor_(editor)
{
}

void Canvas::handleInput(ImVec2 origin, bool hovered, bool active)
{
    const ImGuiIO &io = ImGui::GetIO();
    ImVec2 mouse = ImGui::GetMousePos();
    float localX = mouse.x - origin.x;
    float localY = mouse.y - origin.y;
    int cx = editor_.view().viewport.screenToCelX(localX);
    int cy = editor_.view().viewport.screenToCelY(localY);

    editor_.setHover(cx, cy, hovered);

    if (hovered && io.MouseWheel != 0.0f) {
        float zoom =
            Viewport::nextZoom(editor_.view().viewport.zoom, io.MouseWheel > 0.0f ? 1 : -1);
        editor_.view().viewport.zoomAt(zoom, localX, localY);
        editor_.view().auto_fit = false;
    }
    /* Middle button: a long press without moving is the eyedropper (as long as it is held), a
     * drag pans. */
    if (hovered && !editor_.stroking() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        middle_ = Middle::Pending;
        middlePress_ = mouse;
        middleTime_ = ImGui::GetTime();
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        middle_ = Middle::Idle;
    }
    ImVec2 pan(0.0f, 0.0f);
    if (middle_ == Middle::Pending) {
        float dx = mouse.x - middlePress_.x;
        float dy = mouse.y - middlePress_.y;
        if (dx * dx + dy * dy > MIDDLE_DRAG_PIXELS * MIDDLE_DRAG_PIXELS) {
            middle_ = Middle::Panning;
            pan = ImVec2(dx, dy); /* catch up with what moved before the threshold */
        } else if (ImGui::GetTime() - middleTime_ >= MIDDLE_HOLD_SECONDS) {
            middle_ = Middle::Picking;
        }
    } else if (middle_ == Middle::Panning) {
        pan = io.MouseDelta;
    }
    editor_.tempPick(middle_ == Middle::Picking);
    if (middle_ == Middle::Picking) {
        if (std::optional<Rgba> color =
                eyedropper_pick_sprite(editor_.sprite(), editor_.frame(), cx, cy)) {
            editor_.setForeground(*color);
        }
        return;
    }
    if (middle_ == Middle::Pending) {
        return; /* not decided yet: neither draw nor pan */
    }

    /* The right button pans too, and so does Space + left drag, without drawing. */
    bool spacePan = ImGui::IsKeyDown(ImGuiKey_Space) && !editor_.stroking();
    bool otherPan = active && (ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                               (spacePan && ImGui::IsMouseDown(ImGuiMouseButton_Left)));
    if (otherPan) {
        pan = io.MouseDelta;
    }
    if (middle_ == Middle::Panning || otherPan) {
        editor_.view().viewport.pan_x += pan.x;
        editor_.view().viewport.pan_y += pan.y;
        editor_.view().auto_fit = false;
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        return;
    }

    if (!editor_.stroking() && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left, false)) {
        editor_.press(cx, cy, io.KeyShift);
    } else if (editor_.stroking()) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            editor_.drag(cx, cy, io.KeyShift);
        } else {
            editor_.release();
        }
    }
}

void Canvas::drawGrid(ImDrawList *draw, ImVec2 origin, const ScreenRect &visible) const
{
    const Viewport &view = editor_.view().viewport;
    int firstX = view.screenToCelX(visible.x0 - origin.x);
    int lastX = view.screenToCelX(visible.x1 - origin.x);
    int firstY = view.screenToCelY(visible.y0 - origin.y);
    int lastY = view.screenToCelY(visible.y1 - origin.y);
    ImU32 color = IM_COL32(0, 0, 0, 80);
    for (int x = firstX; x <= lastX; x++) {
        float sx = origin.x + view.celToScreenX(x);
        if (sx >= visible.x0 && sx <= visible.x1) {
            draw->AddLine(ImVec2(sx, visible.y0), ImVec2(sx, visible.y1), color);
        }
    }
    for (int y = firstY; y <= lastY; y++) {
        float sy = origin.y + view.celToScreenY(y);
        if (sy >= visible.y0 && sy <= visible.y1) {
            draw->AddLine(ImVec2(visible.x0, sy), ImVec2(visible.x1, sy), color);
        }
    }
}

/* The cels the pencil / eraser would paint at the hovered cel, as filled squares. */
void Canvas::drawBrushPreview(ImDrawList *draw, ImVec2 origin) const
{
    PencilOptions options = {};
    options.size = editor_.options().brush_size;
    options.round = editor_.options().brush_round;
    int size = options.brushSize();
    int first = options.brushOrigin();
    const Viewport &view = editor_.view().viewport;
    /* Both brush shapes are convex, so each row is one run: a rectangle per row, not per cel. */
    for (int j = 0; j < size; j++) {
        int left = 0;
        while (left < size && !options.brushContains(left, j)) {
            left++;
        }
        int right = size - 1;
        while (right >= left && !options.brushContains(right, j)) {
            right--;
        }
        if (left > right) {
            continue;
        }
        float x0 = origin.x + view.celToScreenX(editor_.hoverX() - first + left);
        float y0 = origin.y + view.celToScreenY(editor_.hoverY() - first + j);
        draw->AddRectFilled(
            ImVec2(x0, y0),
            ImVec2(x0 + view.zoom * static_cast<float>(right - left + 1), y0 + view.zoom),
            IM_COL32(255, 255, 255, 70));
    }
}

void Canvas::drawHoverOutline(ImDrawList *draw, ImVec2 origin) const
{
    if (!editor_.hoverInSprite()) {
        return;
    }
    if ((editor_.tool() == ToolKind::Pencil || editor_.tool() == ToolKind::Eraser) &&
        editor_.options().brush_size > 1) {
        drawBrushPreview(draw, origin);
    }
    float x0 = origin.x + editor_.view().viewport.celToScreenX(editor_.hoverX());
    float y0 = origin.y + editor_.view().viewport.celToScreenY(editor_.hoverY());
    drawRectOutline(draw, ImVec2(x0, y0),
                    ImVec2(x0 + editor_.view().viewport.zoom, y0 + editor_.view().viewport.zoom),
                    IM_COL32(255, 255, 255, 220));
}

/* Selection outline: a dark rectangle under a light one so it shows on any background. */
void Canvas::drawSelection(ImDrawList *draw, ImVec2 origin) const
{
    if (!editor_.hasSelection()) {
        return;
    }
    const Rect &r = editor_.selection();
    int dx = editor_.moveDx();
    int dy = editor_.moveDy();
    ImVec2 p0(origin.x + editor_.view().viewport.celToScreenX(r.x + dx),
              origin.y + editor_.view().viewport.celToScreenY(r.y + dy));
    ImVec2 p1(p0.x + static_cast<float>(r.w) * editor_.view().viewport.zoom,
              p0.y + static_cast<float>(r.h) * editor_.view().viewport.zoom);
    drawRectOutline(draw, ImVec2(p0.x - 1.0f, p0.y - 1.0f), ImVec2(p1.x + 1.0f, p1.y + 1.0f),
                    IM_COL32(0, 0, 0, 220));
    drawRectOutline(draw, p0, p1, IM_COL32(255, 255, 255, 255));
}

/* Center line(s) the symmetry option mirrors across. */
void Canvas::drawMirrorAxes(ImDrawList *draw, const ScreenRect &sprite) const
{
    ImU32 color = IM_COL32(0, 200, 255, 200);
    if (editor_.options().symmetry.mirror_x) {
        float x = (sprite.x0 + sprite.x1) / 2.0f;
        draw->AddLine(ImVec2(x, sprite.y0), ImVec2(x, sprite.y1), color);
    }
    if (editor_.options().symmetry.mirror_y) {
        float y = (sprite.y0 + sprite.y1) / 2.0f;
        draw->AddLine(ImVec2(sprite.x0, y), ImVec2(sprite.x1, y), color);
    }
}

/* Zoom and hovered cel in the canvas corner. Drawn here, after the canvas has applied this frame's
 * zoom / pan / hover, so the numbers never lag the image. */
void Canvas::drawStatus(ImDrawList *draw, ImVec2 origin, ImVec2 avail) const
{
    char text[128];
    const Layer *layer = editor_.sprite().layer(editor_.activeLayer());
    int len = std::snprintf(text, sizeof(text), "%dx%d   Zoom: %g%%   Layer: %s",
                            editor_.sprite().width(), editor_.sprite().height(),
                            static_cast<double>(editor_.view().viewport.zoom) * 100.0,
                            layer ? layer->name().c_str() : "-");
    if (editor_.hoverInSprite()) {
        std::snprintf(text + len, sizeof(text) - static_cast<size_t>(len), "   Cel: %d, %d",
                      editor_.hoverX(), editor_.hoverY());
    }
    ImVec2 size = ImGui::CalcTextSize(text);
    ImVec2 p0(origin.x + 6.0f, origin.y + avail.y - size.y - 10.0f);
    draw->AddRectFilled(ImVec2(p0.x - 4.0f, p0.y - 2.0f),
                        ImVec2(p0.x + size.x + 4.0f, p0.y + size.y + 2.0f), IM_COL32(0, 0, 0, 160));
    draw->AddText(p0, IM_COL32(255, 255, 255, 255), text);
}

/* While the eyedropper is picking: a magnifier beside the cursor showing the cels around it, the
 * cel being picked outlined in the middle, and its color underneath. */
void Canvas::drawLoupe(ImVec2 origin, ImVec2 avail) const
{
    constexpr int RADIUS = 4;
    constexpr int N = 2 * RADIUS + 1;
    constexpr float CELL = 14.0f;
    constexpr float SIZE = static_cast<float>(N) * CELL;
    constexpr float LABEL = 20.0f;
    constexpr float GAP = 22.0f; /* from the cursor */
    constexpr float PAD = 4.0f;
    ImDrawList *draw = ImGui::GetForegroundDrawList();
    ImVec2 mouse = ImGui::GetMousePos();
    float x = mouse.x + GAP;
    float y = mouse.y - GAP - SIZE - LABEL;
    if (x + SIZE + PAD > origin.x + avail.x) {
        x = mouse.x - GAP - SIZE;
    }
    if (y - PAD < origin.y) {
        y = mouse.y + GAP;
    }
    x = std::clamp(x, origin.x + PAD, std::max(origin.x + avail.x - SIZE - PAD, origin.x + PAD));
    y = std::clamp(y, origin.y + PAD,
                   std::max(origin.y + avail.y - SIZE - LABEL - PAD, origin.y + PAD));

    draw->AddRectFilled(ImVec2(x - PAD, y - PAD), ImVec2(x + SIZE + PAD, y + SIZE + LABEL + PAD),
                        IM_COL32(24, 24, 28, 240), 3.0f);
    const int cx = editor_.hoverX();
    const int cy = editor_.hoverY();
    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            ImVec2 p0(x + static_cast<float>(i) * CELL, y + static_cast<float>(j) * CELL);
            ImVec2 p1(p0.x + CELL, p0.y + CELL);
            std::optional<Rgba> color = eyedropper_pick_sprite(editor_.sprite(), editor_.frame(),
                                                               cx - RADIUS + i, cy - RADIUS + j);
            if (!color) {
                draw->AddRectFilled(p0, p1, IM_COL32(48, 48, 54, 255)); /* beside the sprite */
                continue;
            }
            draw->AddRectFilled(p0, p1,
                                (i + j) % 2 == 0 ? IM_COL32(170, 170, 170, 255)
                                                 : IM_COL32(120, 120, 120, 255));
            draw->AddRectFilled(p0, p1, IM_COL32(color->r, color->g, color->b, color->a));
        }
    }
    ImVec2 c0(x + static_cast<float>(RADIUS) * CELL, y + static_cast<float>(RADIUS) * CELL);
    ImVec2 c1(c0.x + CELL, c0.y + CELL);
    draw->AddRect(ImVec2(c0.x - 1.0f, c0.y - 1.0f), ImVec2(c1.x + 1.0f, c1.y + 1.0f),
                  IM_COL32(0, 0, 0, 255));
    draw->AddRect(c0, c1, IM_COL32(255, 255, 255, 255));
    char text[48];
    if (std::optional<Rgba> color =
            eyedropper_pick_sprite(editor_.sprite(), editor_.frame(), cx, cy)) {
        std::snprintf(text, sizeof(text), "#%02X%02X%02X%02X", color->r, color->g, color->b,
                      color->a);
    } else {
        std::snprintf(text, sizeof(text), "-");
    }
    draw->AddText(ImVec2(x, y + SIZE + 3.0f), IM_COL32(255, 255, 255, 255), text);
}

void Canvas::show(float x, float y, float w, float h)
{
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("Canvas", nullptr, flags);
    ImGui::PopStyleVar();

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (editor_.view().auto_fit &&
        (avail.x != editor_.view().canvas_w || avail.y != editor_.view().canvas_h)) {
        editor_.view().needs_fit = true; /* the canvas was resized (window, panel dock / resize) */
    }
    editor_.view().canvas_w = avail.x;
    editor_.view().canvas_h = avail.y;
    int spriteW = editor_.sprite().width();
    int spriteH = editor_.sprite().height();
    if (editor_.view().needs_fit && avail.x > 0.0f && avail.y > 0.0f) {
        editor_.view().viewport.fit(spriteW, spriteH, std::max(avail.x - FIT_MARGIN, 1.0f),
                                    std::max(avail.y - FIT_MARGIN, 1.0f));
        editor_.view().viewport.pan_x += FIT_MARGIN / 2.0f;
        editor_.view().viewport.pan_y += FIT_MARGIN / 2.0f;
        editor_.view().needs_fit = false;
        editor_.view().auto_fit = true;
    }

    ImGui::InvisibleButton("canvas", avail,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
                               ImGuiButtonFlags_MouseButtonRight);
    handleInput(origin, ImGui::IsItemHovered(), ImGui::IsItemActive());
    editor_.updateTexture();

    ImDrawList *draw = ImGui::GetWindowDrawList();
    ImVec2 clipMax(origin.x + avail.x, origin.y + avail.y);
    draw->PushClipRect(origin, clipMax, true);
    draw->AddRectFilled(origin, clipMax, theme::workspaceColor());

    ScreenRect sprite = {origin.x + editor_.view().viewport.celToScreenX(0),
                         origin.y + editor_.view().viewport.celToScreenY(0), 0.0f, 0.0f};
    sprite.x1 = sprite.x0 + static_cast<float>(spriteW) * editor_.view().viewport.zoom;
    sprite.y1 = sprite.y0 + static_cast<float>(spriteH) * editor_.view().viewport.zoom;
    ScreenRect visible = {std::max(sprite.x0, origin.x), std::max(sprite.y0, origin.y),
                          std::min(sprite.x1, clipMax.x), std::min(sprite.y1, clipMax.y)};
    if (visible.x0 < visible.x1 && visible.y0 < visible.y1) {
        drawCheckerboard(draw, visible.x0, visible.y0, visible.x1, visible.y1, sprite.x0,
                         sprite.y0);
        float width = sprite.x1 - sprite.x0;
        float height = sprite.y1 - sprite.y0;
        draw->AddImage(ImTextureRef(reinterpret_cast<ImTextureID>(editor_.texture())),
                       ImVec2(visible.x0, visible.y0), ImVec2(visible.x1, visible.y1),
                       ImVec2((visible.x0 - sprite.x0) / width, (visible.y0 - sprite.y0) / height),
                       ImVec2((visible.x1 - sprite.x0) / width, (visible.y1 - sprite.y0) / height));
        if (editor_.showGrid() && editor_.view().viewport.zoom >= GRID_MIN_ZOOM) {
            drawGrid(draw, origin, visible);
        }
    }
    drawRectOutline(draw, ImVec2(sprite.x0 - 1.0f, sprite.y0 - 1.0f),
                    ImVec2(sprite.x1 + 1.0f, sprite.y1 + 1.0f), IM_COL32(200, 200, 205, 255));
    if (editor_.overlays().mirror_axes) {
        drawMirrorAxes(draw, sprite);
    }
    drawSelection(draw, origin);
    if (editor_.overlays().cursor) {
        drawHoverOutline(draw, origin);
    }
    if (editor_.overlays().status) {
        drawStatus(draw, origin, avail);
    }
    draw->PopClipRect();
    bool picking = middle_ == Middle::Picking ||
                   (editor_.tool() == ToolKind::Eyedropper && editor_.stroking());
    if (picking) {
        drawLoupe(origin, avail);
    }

    ImGui::End();
}
