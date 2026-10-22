#include "slice_overlay.hpp"

#include <algorithm>
#include <cmath>

#include "theme.hpp"

namespace
{

constexpr ImU32 COLOR_SLICE = IM_COL32(76, 201, 240, 255);   /* an unselected slice */
constexpr ImU32 COLOR_SELECTED = IM_COL32(255, 183, 3, 255); /* the selected one, and a new one */
constexpr ImU32 COLOR_PIVOT = IM_COL32(255, 92, 161, 255);
constexpr ImU32 COLOR_SHADOW = IM_COL32(0, 0, 0, 200);
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
                        ImVec2(at.x + size.x + 3.0f, at.y + size.y + 1.0f), IM_COL32(0, 0, 0, 170));
    draw->AddText(ImVec2(at.x + 1.0f, at.y), color, name.c_str());
}

/* A plus sign centered on the pivot cel. */
void pivot(ImDrawList *draw, ImVec2 origin, const Editor &editor, const Sprite::Slice &slice)
{
    const Viewport &view = editor.view().viewport;
    const float half = std::max(4.0f, view.zoom * 0.5f);
    const float cx = origin.x + view.celToScreenX(slice.rect.x + slice.pivotX) + view.zoom / 2.0f;
    const float cy = origin.y + view.celToScreenY(slice.rect.y + slice.pivotY) + view.zoom / 2.0f;
    draw->AddLine(ImVec2(cx - half, cy), ImVec2(cx + half, cy), COLOR_SHADOW, 3.0f);
    draw->AddLine(ImVec2(cx, cy - half), ImVec2(cx, cy + half), COLOR_SHADOW, 3.0f);
    draw->AddLine(ImVec2(cx - half, cy), ImVec2(cx + half, cy), COLOR_PIVOT, 1.0f);
    draw->AddLine(ImVec2(cx, cy - half), ImVec2(cx, cy + half), COLOR_PIVOT, 1.0f);
}

} // namespace

void draw_slices(ImDrawList *draw, ImVec2 origin, const Editor &editor)
{
    const std::vector<Sprite::Slice> &slices = editor.sprite().slices();
    for (int i = 0; i < static_cast<int>(slices.size()); i++) {
        const Sprite::Slice &slice = slices[static_cast<size_t>(i)];
        const bool selected = i == editor.selectedSlice();
        const ImU32 color = selected ? COLOR_SELECTED : COLOR_SLICE;
        const ScreenBox box = boxOf(editor, origin, slice.rect);
        if (selected) {
            draw->AddRectFilled(box.min, box.max, IM_COL32(255, 183, 3, 34));
        }
        outline(draw, box, color, selected ? 2.0f : 1.0f);
        label(draw, box, slice.name, color);
        if (slice.hasPivot) {
            pivot(draw, origin, editor, slice);
        }
    }
    if (const std::optional<Rect> preview = editor.slicePreview()) {
        const ScreenBox box = boxOf(editor, origin, *preview);
        draw->AddRectFilled(box.min, box.max, IM_COL32(255, 183, 3, 34));
        outline(draw, box, COLOR_SELECTED, 1.0f);
    }
}
