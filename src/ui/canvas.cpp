#include "canvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>

#include <string>

#include "ruler.hpp"
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

/* Where the mouse is, for the guides: on the canvas, on one of the two rulers, or elsewhere. */
enum class Place { Canvas, TopRuler, LeftRuler, Elsewhere };

Place placeOf(ImVec2 mouse, ImVec2 origin, ImVec2 avail, float ruler)
{
    const bool insideX = mouse.x >= origin.x && mouse.x < origin.x + avail.x;
    const bool insideY = mouse.y >= origin.y && mouse.y < origin.y + avail.y;
    if (insideX && insideY) {
        return Place::Canvas;
    }
    if (insideX && mouse.y >= origin.y - ruler && mouse.y < origin.y) {
        return Place::TopRuler;
    }
    if (insideY && mouse.x >= origin.x - ruler && mouse.x < origin.x) {
        return Place::LeftRuler;
    }
    return Place::Elsewhere;
}

/* A left press over this window; the canvas button the press just activated does not count as
 * something else being in the way. */
bool clickedLeft()
{
    return ImGui::IsMouseClicked(ImGuiMouseButton_Left, false) &&
           ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
}

ImU32 token(unsigned hex)
{
    return ImGui::ColorConvertFloat4ToU32(theme::rgb(hex));
}

constexpr float RULER_MINOR_GAP = 5.0f;  /* least screen distance between two minor ticks */
constexpr float RULER_MINOR_TICK = 0.3f; /* a minor tick's length as a part of the ruler's width */

/* Cels between two ticks: a fifth or a half of the labelled step, or the step itself when that
 * would crowd the ticks. */
int minorStep(int step, float zoom)
{
    if (step == 1) {
        return 1;
    }
    const int minor = step % 5 == 0 ? step / 5 : step / 2;
    return static_cast<float>(minor) * zoom >= RULER_MINOR_GAP ? minor : step;
}

/* The number of a labelled tick: beside it along a horizontal ruler, one digit under another along
 * a vertical one (text cannot be turned, and the ruler is too narrow for a number lying down). */
void rulerLabel(ImDrawList *draw, bool horizontal, int cel, ImVec2 tick, float thickness)
{
    const std::string text = std::to_string(cel);
    const ImU32 ink = token(theme::MUTE);
    if (horizontal) {
        draw->AddText(ImVec2(tick.x + 3.0f, tick.y - thickness + 1.0f), ink, text.c_str());
        return;
    }
    const float line = ImGui::GetTextLineHeight();
    for (size_t i = 0; i < text.size(); i++) {
        const char *digit = text.c_str() + i;
        const float width = ImGui::CalcTextSize(digit, digit + 1).x;
        draw->AddText(ImVec2(tick.x - thickness + (thickness - width) / 2.0f,
                             tick.y + 2.0f + static_cast<float>(i) * line),
                      ink, digit, digit + 1);
    }
}

/* One ruler: the ticks and labels of the screen range `from`..`to` along its axis, where cel 0
 * starts at `zero`, drawn against the inner edge `edge` of a strip `thickness` wide. */
void drawRulerAxis(ImDrawList *draw, bool horizontal, float zero, float zoom, float from, float to,
                   float edge, float thickness)
{
    const int step = ruler_step(zoom);
    const int unit = minorStep(step, zoom);
    const int first = static_cast<int>(std::floor((from - zero) / zoom / static_cast<float>(unit)));
    const int last = static_cast<int>(std::ceil((to - zero) / zoom / static_cast<float>(unit)));
    for (int k = first; k <= last; k++) {
        const int cel = k * unit;
        const float at = zero + static_cast<float>(cel) * zoom;
        if (at < from || at > to) {
            continue;
        }
        const bool major = cel % step == 0;
        const float length = major ? thickness : thickness * RULER_MINOR_TICK;
        const ImVec2 tick = horizontal ? ImVec2(at, edge) : ImVec2(edge, at);
        const ImVec2 inner = horizontal ? ImVec2(at, edge - length) : ImVec2(edge - length, at);
        draw->AddLine(tick, inner, token(theme::MUTE));
        if (major) {
            rulerLabel(draw, horizontal, cel, tick, thickness);
        }
    }
}

} // namespace

void Canvas::setEditor(Editor &editor)
{
    editor_ = &editor;
    middle_ = Middle::Idle;
    guideDrag_ = GuideDrag();
}

/* The width of the rulers on screen; 0 while they are hidden. */
float Canvas::rulerSize() const
{
    return editor_->overlays().rulers ? theme::px(RULER_THICKNESS) : 0.0f;
}

/*
 * Starts dragging a guide: the one at `index` of the vertical or horizontal list, or, for -1, a
 * new one that begins under the mouse. A new guide switches the guides on so it can be seen.
 */
void Canvas::beginGuide(bool vertical, int index, ImVec2 origin)
{
    if (index < 0) {
        const Viewport &view = editor_->view().viewport;
        const ImVec2 mouse = ImGui::GetMousePos();
        std::vector<int> &lines =
            vertical ? editor_->view().guides.vertical : editor_->view().guides.horizontal;
        lines.push_back(vertical ? guide_snap(view.pan_x, view.zoom, mouse.x - origin.x)
                                 : guide_snap(view.pan_y, view.zoom, mouse.y - origin.y));
        index = static_cast<int>(lines.size()) - 1;
        editor_->overlays().guides = true;
    }
    guideDrag_.vertical = vertical;
    guideDrag_.index = index;
}

/*
 * Follows the mouse with the guide being dragged, snapped to the cel edges. Letting go with the
 * mouse over the ruler (or past it) deletes the guide, which is how a guide is taken away.
 */
void Canvas::dragGuide(ImVec2 origin)
{
    const Viewport &view = editor_->view().viewport;
    std::vector<int> &lines =
        guideDrag_.vertical ? editor_->view().guides.vertical : editor_->view().guides.horizontal;
    const auto index = static_cast<size_t>(guideDrag_.index);
    if (index >= lines.size()) {
        guideDrag_ = GuideDrag();
        return;
    }
    const ImVec2 mouse = ImGui::GetMousePos();
    const float local = guideDrag_.vertical ? mouse.x - origin.x : mouse.y - origin.y;
    ImGui::SetMouseCursor(guideDrag_.vertical ? ImGuiMouseCursor_ResizeEW
                                              : ImGuiMouseCursor_ResizeNS);
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        lines[index] = guide_snap(guideDrag_.vertical ? view.pan_x : view.pan_y, view.zoom, local);
        return;
    }
    if (local < 0.0f) {
        lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(index));
    }
    guideDrag_ = GuideDrag();
}

/* The guide under `mouse`, if the guides are shown and one is within reach. */
Canvas::GuideDrag Canvas::pickGuide(ImVec2 mouse, ImVec2 origin) const
{
    const Viewport &view = editor_->view().viewport;
    const Guides &guides = editor_->view().guides;
    GuideDrag found;
    found.index = guide_pick(guides.vertical, view.pan_x, view.zoom, mouse.x - origin.x);
    found.vertical = found.index >= 0;
    if (!found.vertical) {
        found.index = guide_pick(guides.horizontal, view.pan_y, view.zoom, mouse.y - origin.y);
    }
    return found;
}

/* A press on the guide under the mouse picks it up; hovering it only shows the resize cursor. */
bool Canvas::grabGuide(ImVec2 mouse, ImVec2 origin)
{
    if (!editor_->overlays().guides) {
        return false;
    }
    const GuideDrag over = pickGuide(mouse, origin);
    if (over.index < 0) {
        return false;
    }
    ImGui::SetMouseCursor(over.vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
    if (!clickedLeft()) {
        return false;
    }
    guideDrag_ = over;
    return true;
}

/* A press on a ruler pulls a new guide out of it: the left ruler gives a vertical one. */
bool Canvas::pullGuide(bool vertical, ImVec2 origin)
{
    if (!clickedLeft()) {
        return false;
    }
    beginGuide(vertical, -1, origin);
    return true;
}

/*
 * The guides' share of the mouse this frame: a guide being dragged, a press on a ruler or on a
 * guide. Only while the rulers are shown (so drawing next to a guide never grabs it) and no
 * stroke is going on. True when the mouse belongs to a guide and the tools must not see it.
 */
bool Canvas::guideInput(ImVec2 origin, ImVec2 avail)
{
    if (guideDrag_.index >= 0) {
        dragGuide(origin);
        return true;
    }
    if (!editor_->overlays().rulers || editor_->stroking()) {
        return false;
    }
    const ImVec2 mouse = ImGui::GetMousePos();
    switch (placeOf(mouse, origin, avail, rulerSize())) {
    case Place::Canvas:
        return grabGuide(mouse, origin);
    case Place::TopRuler:
        return pullGuide(false, origin);
    case Place::LeftRuler:
        return pullGuide(true, origin);
    case Place::Elsewhere:
        break;
    }
    return false;
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
    bool spacePan = (ImGui::IsKeyDown(ImGuiKey_Space) || editor_->tool() == ToolKind::Pan) &&
                    !editor_->stroking();
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

void Canvas::handleInput(ImVec2 origin, ImVec2 avail, bool hovered, bool active)
{
    const ImGuiIO &io = ImGui::GetIO();
    ImVec2 mouse = ImGui::GetMousePos();
    float localX = mouse.x - origin.x;
    float localY = mouse.y - origin.y;
    int cx = editor_->view().viewport.screenToCelX(localX);
    int cy = editor_->view().viewport.screenToCelY(localY);

    editor_->setHover(cx, cy, hovered);
    if (guideInput(origin, avail)) {
        return;
    }

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

/*
 * The outline of a selection that is not a rectangle: a line along every side of a selected cel

 * * whose neighbor is not selected, dark under light so it shows on any background. The sides are

 * * found once per mask (see outline_). A selection with a great many (a dust of single pixels) is

 * * shown as its bounding rectangle instead, so the outline cannot slow the frame down.
 */
void Canvas::refreshOutline(const std::shared_ptr<const Mask> &mask) const
{
    if (outlineOf_ == mask) {
        return;
    }
    outlineOf_ = mask;
    outline_.clear();
    mask->forEach([&](int x, int y) {
        if (outline_.size() > MAX_OUTLINE_SIDES) {
            return;
        }
        if (!mask->contains(x, y - 1)) {
            outline_.push_back({x, y, x + 1, y});
        }
        if (!mask->contains(x, y + 1)) {
            outline_.push_back({x, y + 1, x + 1, y + 1});
        }
        if (!mask->contains(x - 1, y)) {
            outline_.push_back({x, y, x, y + 1});
        }
        if (!mask->contains(x + 1, y)) {
            outline_.push_back({x + 1, y, x + 1, y + 1});
        }
    });
    outlineTooBig_ = outline_.size() > MAX_OUTLINE_SIDES;
}

void Canvas::drawMaskOutline(ImDrawList *draw, ImVec2 origin,
                             const std::shared_ptr<const Mask> &mask) const
{
    const Viewport &view = editor_->view().viewport;
    refreshOutline(mask);
    const int dx = editor_->moveDx();
    const int dy = editor_->moveDy();
    if (outlineTooBig_) {
        const Rect b = mask->bounds();
        const ImVec2 p0(origin.x + view.celToScreenX(b.x + dx),
                        origin.y + view.celToScreenY(b.y + dy));
        const ImVec2 p1(p0.x + static_cast<float>(b.w) * view.zoom,
                        p0.y + static_cast<float>(b.h) * view.zoom);
        drawRectOutline(draw, p0, p1, theme::overlay::LINE_LIGHT);
        return;
    }
    auto screen = [&](int x, int y) {
        return ImVec2(origin.x + view.celToScreenX(x + dx), origin.y + view.celToScreenY(y + dy));
    };
    for (const Side &side : outline_) {
        draw->AddLine(screen(side.x0, side.y0), screen(side.x1, side.y1),
                      theme::overlay::LINE_SHADOW, 3.0f);
    }
    for (const Side &side : outline_) {
        draw->AddLine(screen(side.x0, side.y0), screen(side.x1, side.y1),
                      theme::overlay::LINE_LIGHT, 1.0f);
    }
}
/* The lasso while it is dragged: the path through the middle of the cels it visited, and a fainter
 * line closing it, which is the outline the selection will get. */
void Canvas::drawLasso(ImDrawList *draw, ImVec2 origin) const
{
    const std::vector<ToolPoint> &path = editor_->lassoPath();
    if (path.size() < 2) {
        return;
    }
    const Viewport &view = editor_->view().viewport;
    std::vector<ImVec2> points;
    points.reserve(path.size());
    for (const ToolPoint &p : path) {
        points.emplace_back(origin.x + view.celToScreenX(p.x) + view.zoom / 2.0f,
                            origin.y + view.celToScreenY(p.y) + view.zoom / 2.0f);
    }
    const int count = static_cast<int>(points.size());
    draw->AddPolyline(points.data(), count, theme::overlay::LINE_SHADOW, ImDrawFlags_None, 3.0f);
    draw->AddPolyline(points.data(), count, theme::overlay::LINE_LIGHT, ImDrawFlags_None, 1.0f);
    draw->AddLine(points.back(), points.front(), theme::overlay::LINE_SHADOW, 3.0f);
    draw->AddLine(points.back(), points.front(), theme::overlay::LINE_LIGHT & 0x80ffffffu, 1.0f);
}

/* Selection outline: a dark rectangle under a light one so it shows on any background. */
void Canvas::drawSelection(ImDrawList *draw, ImVec2 origin) const
{
    drawLasso(draw, origin);
    if (!editor_->hasSelection()) {
        return;
    }
    if (const std::shared_ptr<const Mask> mask = editor_->selectionMask()) {
        drawMaskOutline(draw, origin, mask);
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

/* The guides across the whole canvas, each a thin reddish line. */
void Canvas::drawGuides(ImDrawList *draw, ImVec2 origin, ImVec2 avail) const
{
    const Viewport &view = editor_->view().viewport;
    const Guides &guides = editor_->view().guides;
    const ImVec2 end(origin.x + avail.x, origin.y + avail.y);
    for (const int x : guides.vertical) {
        const float sx = origin.x + view.celToScreenX(x);
        draw->AddLine(ImVec2(sx, origin.y), ImVec2(sx, end.y), theme::overlay::GUIDE);
    }
    for (const int y : guides.horizontal) {
        const float sy = origin.y + view.celToScreenY(y);
        draw->AddLine(ImVec2(origin.x, sy), ImVec2(end.x, sy), theme::overlay::GUIDE);
    }
}

/* The two rulers in the strips above and left of the canvas, and the square where they meet. */
void Canvas::drawRulers(ImDrawList *draw, ImVec2 windowOrigin, ImVec2 origin, ImVec2 avail) const
{
    const Viewport &view = editor_->view().viewport;
    const float thickness = origin.x - windowOrigin.x;
    const ImVec2 end(origin.x + avail.x, origin.y + avail.y);
    draw->PushClipRect(windowOrigin, end, true);
    draw->AddRectFilled(windowOrigin, ImVec2(end.x, origin.y), token(theme::CANVAS_SOFT));
    draw->AddRectFilled(ImVec2(windowOrigin.x, origin.y), ImVec2(origin.x, end.y),
                        token(theme::CANVAS_SOFT));
    draw->AddLine(ImVec2(origin.x, origin.y), ImVec2(end.x, origin.y), token(theme::HAIRLINE));
    draw->AddLine(ImVec2(origin.x, origin.y), ImVec2(origin.x, end.y), token(theme::HAIRLINE));
    drawRulerAxis(draw, true, origin.x + view.pan_x, view.zoom, origin.x, end.x, origin.y,
                  thickness);
    drawRulerAxis(draw, false, origin.y + view.pan_y, view.zoom, origin.y, end.y, origin.x,
                  thickness);
    draw->PopClipRect();
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
    if (editor_->overlays().guides) {
        drawGuides(draw, origin, avail);
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

    const ImVec2 windowOrigin = ImGui::GetCursorScreenPos();
    const float ruler = rulerSize();
    const ImVec2 origin(windowOrigin.x + ruler, windowOrigin.y + ruler);
    ImGui::SetCursorScreenPos(origin);
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
    handleInput(origin, avail, ImGui::IsItemHovered(), ImGui::IsItemActive());
    editor_->updateTexture();
    drawContent(origin, avail);
    if (ruler > 0.0f) {
        drawRulers(ImGui::GetWindowDrawList(), windowOrigin, origin, avail);
    }
    bool picking = middle_ == Middle::Picking ||
                   (editor_->tool() == ToolKind::Eyedropper && editor_->stroking());
    if (picking) {
        drawLoupe(origin, avail);
    }
    if (highlighted_) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            windowOrigin, ImVec2(origin.x + avail.x, windowOrigin.y + theme::px(2.0f)),
            ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::ACCENT, 0.85f)));
    }

    ImGui::End();
}
