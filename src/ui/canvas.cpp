#include "canvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>

#include "theme.hpp"

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
    draw->AddRectFilled(ImVec2(vx0, vy0), ImVec2(vx1, vy1), theme::overlay::CHECKER_LIGHT);
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
                draw->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), theme::overlay::CHECKER_TILE);
            }
        }
    }
}

void drawRectOutline(ImDrawList *draw, ImVec2 p0, ImVec2 p1, ImU32 color)
{
    draw->AddRect(p0, p1, color);
}

constexpr int LOUPE_RADIUS = 4;
constexpr int LOUPE_CELLS = 2 * LOUPE_RADIUS + 1;
constexpr float LOUPE_CELL = 14.0f;
constexpr float LOUPE_SIZE = static_cast<float>(LOUPE_CELLS) * LOUPE_CELL;
constexpr float LOUPE_LABEL = 20.0f;
constexpr float LOUPE_GAP = 22.0f; /* from the cursor */
constexpr float LOUPE_PAD = 4.0f;

/* Top-left of the loupe: above and right of the cursor, flipped or clamped to stay inside the
 * canvas. */
ImVec2 loupePosition(ImVec2 mouse, ImVec2 origin, ImVec2 avail)
{
    float x = mouse.x + LOUPE_GAP;
    float y = mouse.y - LOUPE_GAP - LOUPE_SIZE - LOUPE_LABEL;
    if (x + LOUPE_SIZE + LOUPE_PAD > origin.x + avail.x) {
        x = mouse.x - LOUPE_GAP - LOUPE_SIZE;
    }
    if (y - LOUPE_PAD < origin.y) {
        y = mouse.y + LOUPE_GAP;
    }
    x = std::clamp(x, origin.x + LOUPE_PAD,
                   std::max(origin.x + avail.x - LOUPE_SIZE - LOUPE_PAD, origin.x + LOUPE_PAD));
    y = std::clamp(
        y, origin.y + LOUPE_PAD,
        std::max(origin.y + avail.y - LOUPE_SIZE - LOUPE_LABEL - LOUPE_PAD, origin.y + LOUPE_PAD));
    return {x, y};
}

constexpr ImU32 COLOR_SLICE = theme::overlay::SLICE; /* an unselected slice */
constexpr ImU32 COLOR_SELECTED =
    theme::overlay::SLICE_SELECTED; /* the selected one, and a new one */
constexpr ImU32 COLOR_PIVOT = theme::overlay::SLICE_PIVOT;
constexpr ImU32 COLOR_SHADOW = theme::overlay::SLICE_SHADOW;
constexpr float LABEL_MIN_WIDTH =
    28.0f; /* no label on a slice that is smaller than this on screen */

struct ScreenBox {
    ImVec2 min;
    ImVec2 max;
};

ScreenBox boxOf(const Editor &editor, ImVec2 origin, Rect r)
{
    const Viewport &view = editor.view().viewport;
    const ImVec2 min(origin.x + view.celToScreenX(r.x), origin.y + view.celToScreenY(r.y));
    return {min, ImVec2(min.x + static_cast<float>(r.w) * view.zoom,
                        min.y + static_cast<float>(r.h) * view.zoom)};
}

void outline(ImDrawList *draw, const ScreenBox &box, ImU32 color, float thickness)
{
    draw->AddRect(ImVec2(box.min.x - 1.0f, box.min.y - 1.0f),
                  ImVec2(box.max.x + 1.0f, box.max.y + 1.0f), COLOR_SHADOW, 0.0f, 0, thickness);
    draw->AddRect(box.min, box.max, color, 0.0f, 0, thickness);
}

/* The name above the top-left corner, on a dark strip so it reads over any pixels. */
void label(ImDrawList *draw, const ScreenBox &box, const std::string &name, ImU32 color)
{
    if (box.max.x - box.min.x < theme::px(LABEL_MIN_WIDTH)) {
        return;
    }
    const ImVec2 size = ImGui::CalcTextSize(name.c_str());
    const ImVec2 at(box.min.x, box.min.y - size.y - 3.0f);
    draw->AddRectFilled(ImVec2(at.x - 1.0f, at.y - 1.0f),
                        ImVec2(at.x + size.x + 3.0f, at.y + size.y + 1.0f),
                        theme::overlay::SLICE_LABEL_BACKGROUND);
    draw->AddText(ImVec2(at.x + 1.0f, at.y), color, name.c_str());
}

/* A plus sign on the pivot: the grid point `pivotX`, `pivotY` cels from the slice's top-left
 * corner, which is the point the sheet JSON gives (times the export scale). `at` is where the slice
 * is drawn (it differs from its rectangle while it is being moved). */
void pivot(ImDrawList *draw, ImVec2 origin, const Editor &editor, const Sprite::Slice &slice,
           Rect at)
{
    const Viewport &view = editor.view().viewport;
    const float half = std::max(4.0f, view.zoom * 0.5f);
    const float cx = origin.x + view.celToScreenX(at.x + slice.pivotX);
    const float cy = origin.y + view.celToScreenY(at.y + slice.pivotY);
    draw->AddLine(ImVec2(cx - half, cy), ImVec2(cx + half, cy), COLOR_SHADOW, 3.0f);
    draw->AddLine(ImVec2(cx, cy - half), ImVec2(cx, cy + half), COLOR_SHADOW, 3.0f);
    draw->AddLine(ImVec2(cx - half, cy), ImVec2(cx + half, cy), COLOR_PIVOT, 1.0f);
    draw->AddLine(ImVec2(cx, cy - half), ImVec2(cx, cy + half), COLOR_PIVOT, 1.0f);
}

/* Draws the sprite's slices over the canvas: an outline and the name for each (the selected one in
 * another color and lightly filled), a cross at a pivot, and the rectangle being dragged out.
 * `origin` is the top-left of the canvas window; the editor's viewport maps cels to it. */
void drawSlices(ImDrawList *draw, ImVec2 origin, const Editor &editor)
{
    const std::vector<Sprite::Slice> &slices = editor.sprite().slices();
    for (int i = 0; i < static_cast<int>(slices.size()); i++) {
        const Sprite::Slice &slice = slices[static_cast<size_t>(i)];
        const bool selected = i == editor.selectedSlice();
        const ImU32 color = selected ? COLOR_SELECTED : COLOR_SLICE;
        /* a slice that is being moved is drawn where the drag has it */
        const Rect at = editor.sliceMovePreview(i).value_or(slice.rect);
        const ScreenBox box = boxOf(editor, origin, at);
        if (selected) {
            draw->AddRectFilled(box.min, box.max, theme::overlay::SLICE_SELECTED_FILL);
        }
        outline(draw, box, color, selected ? 2.0f : 1.0f);
        label(draw, box, slice.name, color);
        if (slice.hasPivot) {
            pivot(draw, origin, editor, slice, at);
        }
    }
    if (const std::optional<Rect> preview = editor.slicePreview()) {
        const ScreenBox box = boxOf(editor, origin, *preview);
        draw->AddRectFilled(box.min, box.max, theme::overlay::SLICE_SELECTED_FILL);
        outline(draw, box, COLOR_SELECTED, 1.0f);
    }
}

} // namespace

void Canvas::setEditor(Editor &editor)
{
    editor_ = &editor;
    middle_ = Middle::Idle;
}

Canvas::Canvas(Editor &editor, const char *windowName) : editor_(&editor), windowName_(windowName)
{
}

/* Advances the middle button state and returns how far to pan this frame. */
ImVec2 Canvas::updateMiddle(ImVec2 mouse, bool hovered)
{
    if (hovered && !editor_->stroking() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
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
        pan = ImGui::GetIO().MouseDelta;
    }
    return pan;
}

/* Pans by `pan` (from the middle button) or, with the right button or Space + left drag, by the
 * mouse movement. True when the canvas panned, which also means nothing is drawn this frame. */
bool Canvas::applyPan(ImVec2 pan, bool active)
{
    bool spacePan = ImGui::IsKeyDown(ImGuiKey_Space) && !editor_->stroking();
    bool otherPan = active && (ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                               (spacePan && ImGui::IsMouseDown(ImGuiMouseButton_Left)));
    if (otherPan) {
        pan = ImGui::GetIO().MouseDelta;
    }
    if (middle_ != Middle::Panning && !otherPan) {
        return false;
    }
    editor_->view().viewport.pan_x += pan.x;
    editor_->view().viewport.pan_y += pan.y;
    editor_->view().auto_fit = false;
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return true;
}

/* The left button: press starts a stroke, holding drags it, letting go ends it. */
void Canvas::strokeInput(int cx, int cy, bool hovered)
{
    bool shift = ImGui::GetIO().KeyShift;
    if (!editor_->stroking() && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left, false)) {
        editor_->press(cx, cy, shift);
    } else if (editor_->stroking()) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            editor_->drag(cx, cy, shift);
        } else {
            editor_->release();
        }
    }
}

void Canvas::handleInput(ImVec2 origin, bool hovered, bool active)
{
    const ImGuiIO &io = ImGui::GetIO();
    ImVec2 mouse = ImGui::GetMousePos();
    float localX = mouse.x - origin.x;
    float localY = mouse.y - origin.y;
    int cx = editor_->view().viewport.screenToCelX(localX);
    int cy = editor_->view().viewport.screenToCelY(localY);

    editor_->setHover(cx, cy, hovered);

    if (hovered && io.MouseWheel != 0.0f) {
        float zoom =
            Viewport::nextZoom(editor_->view().viewport.zoom, io.MouseWheel > 0.0f ? 1 : -1);
        editor_->view().viewport.zoomAt(zoom, localX, localY);
        editor_->view().auto_fit = false;
    }
    /* Middle button: a long press without moving is the eyedropper (as long as it is held), a
     * drag pans. */
    ImVec2 pan = updateMiddle(mouse, hovered);
    editor_->tempPick(middle_ == Middle::Picking);
    if (middle_ == Middle::Picking) {
        if (std::optional<Rgba> color =
                eyedropper_pick_sprite(editor_->sprite(), editor_->frame(), cx, cy)) {
            editor_->setForeground(*color);
        }
        return;
    }
    if (middle_ == Middle::Pending) {
        return; /* not decided yet: neither draw nor pan */
    }
    if (!applyPan(pan, active)) {
        strokeInput(cx, cy, hovered);
    }
}

void Canvas::drawGrid(ImDrawList *draw, ImVec2 origin, const ScreenRect &visible) const
{
    const Viewport &view = editor_->view().viewport;
    int firstX = view.screenToCelX(visible.x0 - origin.x);
    int lastX = view.screenToCelX(visible.x1 - origin.x);
    int firstY = view.screenToCelY(visible.y0 - origin.y);
    int lastY = view.screenToCelY(visible.y1 - origin.y);
    ImU32 color = theme::overlay::PIXEL_GRID;
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
    PencilOptions options = editor_->options().pencil();
    int size = options.brushSize();
    int first = options.brushOrigin();
    const Viewport &view = editor_->view().viewport;
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
        float x0 = origin.x + view.celToScreenX(editor_->hoverX() - first + left);
        float y0 = origin.y + view.celToScreenY(editor_->hoverY() - first + j);
        draw->AddRectFilled(
            ImVec2(x0, y0),
            ImVec2(x0 + view.zoom * static_cast<float>(right - left + 1), y0 + view.zoom),
            theme::overlay::ROW_HIGHLIGHT);
    }
}

void Canvas::drawHoverOutline(ImDrawList *draw, ImVec2 origin) const
{
    if (!editor_->hoverInSprite()) {
        return;
    }
    if (tool_traits(editor_->tool()).group == ToolGroup::Brush &&
        editor_->options().brush_size > 1) {
        drawBrushPreview(draw, origin);
    }
    float x0 = origin.x + editor_->view().viewport.celToScreenX(editor_->hoverX());
    float y0 = origin.y + editor_->view().viewport.celToScreenY(editor_->hoverY());
    drawRectOutline(draw, ImVec2(x0, y0),
                    ImVec2(x0 + editor_->view().viewport.zoom, y0 + editor_->view().viewport.zoom),
                    theme::overlay::PIXEL_OUTLINE);
}

/* Selection outline: a dark rectangle under a light one so it shows on any background. */
void Canvas::drawSelection(ImDrawList *draw, ImVec2 origin) const
{
    if (!editor_->hasSelection()) {
        return;
    }
    const Rect &r = editor_->selection();
    int dx = editor_->moveDx();
    int dy = editor_->moveDy();
    ImVec2 p0(origin.x + editor_->view().viewport.celToScreenX(r.x + dx),
              origin.y + editor_->view().viewport.celToScreenY(r.y + dy));
    ImVec2 p1(p0.x + static_cast<float>(r.w) * editor_->view().viewport.zoom,
              p0.y + static_cast<float>(r.h) * editor_->view().viewport.zoom);
    drawRectOutline(draw, ImVec2(p0.x - 1.0f, p0.y - 1.0f), ImVec2(p1.x + 1.0f, p1.y + 1.0f),
                    theme::overlay::LINE_SHADOW);
    drawRectOutline(draw, p0, p1, theme::overlay::LINE_LIGHT);
}

/* Center line(s) the symmetry option mirrors across. */
void Canvas::drawMirrorAxes(ImDrawList *draw, const ScreenRect &sprite) const
{
    ImU32 color = theme::overlay::MIRROR_AXIS;
    if (editor_->options().symmetry.mirror_x) {
        float x = (sprite.x0 + sprite.x1) / 2.0f;
        draw->AddLine(ImVec2(x, sprite.y0), ImVec2(x, sprite.y1), color);
    }
    if (editor_->options().symmetry.mirror_y) {
        float y = (sprite.y0 + sprite.y1) / 2.0f;
        draw->AddLine(ImVec2(sprite.x0, y), ImVec2(sprite.x1, y), color);
    }
}

/* While the eyedropper is picking: a magnifier beside the cursor showing the cels around it, the
 * cel being picked outlined in the middle, and its color underneath. */
void Canvas::drawLoupe(ImVec2 origin, ImVec2 avail) const
{
    ImDrawList *draw = ImGui::GetForegroundDrawList();
    ImVec2 at = loupePosition(ImGui::GetMousePos(), origin, avail);
    draw->AddRectFilled(
        ImVec2(at.x - LOUPE_PAD, at.y - LOUPE_PAD),
        ImVec2(at.x + LOUPE_SIZE + LOUPE_PAD, at.y + LOUPE_SIZE + LOUPE_LABEL + LOUPE_PAD),
        theme::overlay::LOUPE_BACKGROUND, 3.0f);
    const int cx = editor_->hoverX();
    const int cy = editor_->hoverY();
    for (int j = 0; j < LOUPE_CELLS; j++) {
        for (int i = 0; i < LOUPE_CELLS; i++) {
            ImVec2 p0(at.x + static_cast<float>(i) * LOUPE_CELL,
                      at.y + static_cast<float>(j) * LOUPE_CELL);
            ImVec2 p1(p0.x + LOUPE_CELL, p0.y + LOUPE_CELL);
            std::optional<Rgba> color = eyedropper_pick_sprite(
                editor_->sprite(), editor_->frame(), cx - LOUPE_RADIUS + i, cy - LOUPE_RADIUS + j);
            if (!color) {
                draw->AddRectFilled(p0, p1, theme::overlay::LOUPE_OUTSIDE); /* beside the sprite */
                continue;
            }
            draw->AddRectFilled(p0, p1,
                                (i + j) % 2 == 0 ? theme::overlay::CHECKER_LIGHT
                                                 : theme::overlay::CHECKER_DARK);
            draw->AddRectFilled(p0, p1, IM_COL32(color->r, color->g, color->b, color->a));
        }
    }
    ImVec2 c0(at.x + static_cast<float>(LOUPE_RADIUS) * LOUPE_CELL,
              at.y + static_cast<float>(LOUPE_RADIUS) * LOUPE_CELL);
    ImVec2 c1(c0.x + LOUPE_CELL, c0.y + LOUPE_CELL);
    draw->AddRect(ImVec2(c0.x - 1.0f, c0.y - 1.0f), ImVec2(c1.x + 1.0f, c1.y + 1.0f),
                  theme::overlay::LINE_DARK);
    draw->AddRect(c0, c1, theme::overlay::LINE_LIGHT);
    char text[48];
    if (std::optional<Rgba> color =
            eyedropper_pick_sprite(editor_->sprite(), editor_->frame(), cx, cy)) {
        std::snprintf(text, sizeof(text), "#%02X%02X%02X%02X", color->r, color->g, color->b,
                      color->a);
    } else {
        std::snprintf(text, sizeof(text), "-");
    }
    draw->AddText(ImVec2(at.x, at.y + LOUPE_SIZE + 3.0f), theme::overlay::LINE_LIGHT, text);
}

/* Re-fits the sprite when asked to, or when the canvas was resized while it follows the window. */
void Canvas::updateFit(ImVec2 avail)
{
    if (editor_->view().auto_fit &&
        (avail.x != editor_->view().canvas_w || avail.y != editor_->view().canvas_h)) {
        editor_->view().needs_fit = true; /* the canvas was resized (window, panel dock / resize) */
    }
    editor_->view().canvas_w = avail.x;
    editor_->view().canvas_h = avail.y;
    if (editor_->view().needs_fit && avail.x > 0.0f && avail.y > 0.0f) {
        editor_->view().viewport.fit(editor_->sprite().width(), editor_->sprite().height(),
                                     std::max(avail.x - FIT_MARGIN, 1.0f),
                                     std::max(avail.y - FIT_MARGIN, 1.0f));
        editor_->view().viewport.pan_x += FIT_MARGIN / 2.0f;
        editor_->view().viewport.pan_y += FIT_MARGIN / 2.0f;
        editor_->view().needs_fit = false;
        editor_->view().auto_fit = true;
    }
}

/* The workspace, the sprite over its checkerboard, the grid and the overlays, clipped to the canvas
 * window. */
void Canvas::drawContent(ImVec2 origin, ImVec2 avail) const
{
    ImDrawList *draw = ImGui::GetWindowDrawList();
    ImVec2 clipMax(origin.x + avail.x, origin.y + avail.y);
    draw->PushClipRect(origin, clipMax, true);
    draw->AddRectFilled(origin, clipMax, theme::workspaceColor());

    ScreenRect sprite = {origin.x + editor_->view().viewport.celToScreenX(0),
                         origin.y + editor_->view().viewport.celToScreenY(0), 0.0f, 0.0f};
    sprite.x1 =
        sprite.x0 + static_cast<float>(editor_->sprite().width()) * editor_->view().viewport.zoom;
    sprite.y1 =
        sprite.y0 + static_cast<float>(editor_->sprite().height()) * editor_->view().viewport.zoom;
    ScreenRect visible = {std::max(sprite.x0, origin.x), std::max(sprite.y0, origin.y),
                          std::min(sprite.x1, clipMax.x), std::min(sprite.y1, clipMax.y)};
    if (visible.x0 < visible.x1 && visible.y0 < visible.y1) {
        drawCheckerboard(draw, visible.x0, visible.y0, visible.x1, visible.y1, sprite.x0,
                         sprite.y0);
        float width = sprite.x1 - sprite.x0;
        float height = sprite.y1 - sprite.y0;
        theme::pixel_image(
            draw, ImTextureRef(reinterpret_cast<ImTextureID>(editor_->texture())),
            ImVec2(visible.x0, visible.y0), ImVec2(visible.x1, visible.y1),
            ImVec2((visible.x0 - sprite.x0) / width, (visible.y0 - sprite.y0) / height),
            ImVec2((visible.x1 - sprite.x0) / width, (visible.y1 - sprite.y0) / height));
        if (editor_->showGrid() && editor_->view().viewport.zoom >= GRID_MIN_ZOOM) {
            drawGrid(draw, origin, visible);
        }
    }
    drawRectOutline(draw, ImVec2(sprite.x0 - 1.0f, sprite.y0 - 1.0f),
                    ImVec2(sprite.x1 + 1.0f, sprite.y1 + 1.0f), theme::overlay::SPRITE_FRAME);
    if (editor_->overlays().mirror_axes) {
        drawMirrorAxes(draw, sprite);
    }
    drawSelection(draw, origin);
    if (editor_->overlays().slices) {
        drawSlices(draw, origin, *editor_);
    }
    if (editor_->overlays().cursor) {
        drawHoverOutline(draw, origin);
    }
    draw->PopClipRect();
}

void Canvas::show(float x, float y, float w, float h)
{
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin(windowName_.c_str(), nullptr, flags);
    ImGui::PopStyleVar();

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    updateFit(avail);
    ImGui::InvisibleButton("canvas", avail,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
                               ImGuiButtonFlags_MouseButtonRight);
    if (onPress_ && ImGui::IsItemHovered() &&
        (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
         ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
         ImGui::IsMouseClicked(ImGuiMouseButton_Middle))) {
        onPress_();
    }
    handleInput(origin, ImGui::IsItemHovered(), ImGui::IsItemActive());
    editor_->updateTexture();
    drawContent(origin, avail);
    bool picking = middle_ == Middle::Picking ||
                   (editor_->tool() == ToolKind::Eyedropper && editor_->stroking());
    if (picking) {
        drawLoupe(origin, avail);
    }
    if (highlighted_) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            origin, ImVec2(origin.x + avail.x, origin.y + theme::px(2.0f)),
            ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::ACCENT, 0.85f)));
    }

    ImGui::End();
}
