#include "icons.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "theme.hpp"

namespace icons
{

namespace
{

constexpr size_t ICON_COUNT = static_cast<size_t>(Icon::Count);

/* Indexed by Icon; the comment is the Material Symbols name. */
constexpr std::array<unsigned, ICON_COUNT> CODEPOINTS = {
    0xe8f4, /* Eye: visibility */
    0xe8f5, /* EyeOff: visibility_off */
    0xe899, /* Lock: lock */
    0xe898, /* Unlock: lock_open */
    0xe145, /* Plus: add */
    0xe14d, /* Copy: content_copy */
    0xe92e, /* Trash: delete */
    0xe258, /* Merge: vertical_align_bottom */
    0xf41e, /* Line: diagonal_line */
    0xe3c6, /* Rect: crop_square */
    0xe836, /* Ellipse: radio_button_unchecked */
    0xe86b, /* Triangle: change_history */
    0,      /* Diamond: drawn with lines, Material Symbols has no plain rhombus */
    0xeb39, /* Hexagon: hexagon */
    0xf09a, /* Star: star */
    0xf2c3, /* Cross: health_cross */
    0xe15b, /* Minus: remove */
    0xe5d5, /* Reset: refresh */
    0xe5d8, /* ArrowUp: arrow_upward */
    0xe5db, /* ArrowDown: arrow_downward */
    0xf097, /* Pencil: edit */
    0xe6d0, /* Eraser: ink_eraser */
    0xe23a, /* Bucket: format_color_fill */
    0xe3b8, /* Eyedropper: colorize */
    0xef52, /* Select: highlight_alt */
    0xe8d4, /* Swap: swap_horiz */
    0,      /* Folder: drawn with lines */
    0,      /* Slice: drawn with lines */
};

constexpr float ROUND = 6.0f;
constexpr float GLYPH_SIZE_BASE = 20.0f;
constexpr float ROUND_GLYPH_RATIO = 0.8f; /* of the square-button glyph */

ImU32 withAlpha(unsigned hex, float alpha)
{
    return ImGui::ColorConvertFloat4ToU32(theme::rgb(hex, alpha));
}

/* `codepoint` as UTF-8 (the Private Use Area is always three bytes). */
std::array<char, 4> utf8(unsigned codepoint)
{
    return {static_cast<char>(0xE0 | (codepoint >> 12)),
            static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)),
            static_cast<char>(0x80 | (codepoint & 0x3F)), 0};
}

} // namespace

unsigned codepoint(Icon icon)
{
    return CODEPOINTS[static_cast<size_t>(icon)];
}

bool is_drawn(Icon icon)
{
    return icon == Icon::Diamond || icon == Icon::Folder || icon == Icon::Slice;
}

float glyph_size(float uiScale)
{
    return std::round(GLYPH_SIZE_BASE * uiScale);
}

float round_glyph_size(float uiScale)
{
    return std::round(glyph_size(uiScale) * ROUND_GLYPH_RATIO);
}

ImVec2 glyph_origin(ImVec2 min, ImVec2 max, float x0, float y0, float x1, float y1)
{
    return {std::floor((min.x + max.x) / 2.0f - (x0 + x1) / 2.0f),
            std::floor((min.y + max.y) / 2.0f - (y0 + y1) / 2.0f)};
}

/* A rhombus outline, as thick as a Material Symbols stroke at the same size. */
void drawDiamond(ImDrawList *list, ImVec2 min, ImVec2 max, ImU32 color)
{
    const float size = glyph_size(theme::scale());
    const float half = std::round(size * 0.36f);
    const float cx = std::floor((min.x + max.x) / 2.0f) + 0.5f;
    const float cy = std::floor((min.y + max.y) / 2.0f) + 0.5f;
    const ImVec2 points[4] = {ImVec2(cx, cy - half), ImVec2(cx + half, cy), ImVec2(cx, cy + half),
                              ImVec2(cx - half, cy)};
    list->AddPolyline(points, 4, color, ImDrawFlags_Closed, size * 0.07f);
}

/* A folder outline (a body with a tab), as thick as a Material Symbols stroke at the same size. */
void drawFolder(ImDrawList *list, ImVec2 min, ImVec2 max, ImU32 color)
{
    const float size = glyph_size(theme::scale());
    const float w = std::round(size * 0.85f);
    const float h = std::round(size * 0.65f);
    const float left = std::floor((min.x + max.x - w) / 2.0f) + 0.5f;
    const float top = std::floor((min.y + max.y - h) / 2.0f) + 0.5f;
    const float tab = std::round(h * 0.22f);
    const ImVec2 points[6] = {
        ImVec2(left, top + h),
        ImVec2(left, top),
        ImVec2(left + std::round(w * 0.4f), top),
        ImVec2(left + std::round(w * 0.5f), top + tab),
        ImVec2(left + w, top + tab),
        ImVec2(left + w, top + h),
    };
    list->AddPolyline(points, 6, color, ImDrawFlags_Closed, size * 0.07f);
}

/* Four corner brackets around a small square: a rectangle cut out of the picture. */
void drawSliceIcon(ImDrawList *list, ImVec2 min, ImVec2 max, ImU32 color)
{
    const float size = glyph_size(theme::scale());
    const float half = std::round(size * 0.38f);
    const float arm = std::round(size * 0.22f);
    const float cx = std::floor((min.x + max.x) / 2.0f) + 0.5f;
    const float cy = std::floor((min.y + max.y) / 2.0f) + 0.5f;
    const float thickness = size * 0.07f;
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            const float x = cx + static_cast<float>(sx) * half;
            const float y = cy + static_cast<float>(sy) * half;
            const ImVec2 points[3] = {ImVec2(x - static_cast<float>(sx) * arm, y), ImVec2(x, y),
                                      ImVec2(x, y - static_cast<float>(sy) * arm)};
            list->AddPolyline(points, 3, color, ImDrawFlags_None, thickness);
        }
    }
    list->AddRectFilled(ImVec2(cx - 1.5f, cy - 1.5f), ImVec2(cx + 1.5f, cy + 1.5f), color);
}

namespace
{

/* The glyph of `icon` at `size` pixels, centered in [min, max] by its ink. */
void drawGlyph(ImDrawList *list, Icon icon, ImVec2 min, ImVec2 max, ImU32 color, float size)
{
    ImFont *font = ImGui::GetFont();
    const ImFontGlyph *glyph =
        font->GetFontBaked(size)->FindGlyph(static_cast<ImWchar>(codepoint(icon)));
    if (glyph == nullptr || !glyph->Visible) {
        return;
    }
    ImVec2 at = glyph_origin(min, max, glyph->X0, glyph->Y0, glyph->X1, glyph->Y1);
    const std::array<char, 4> text = utf8(codepoint(icon));
    list->AddText(font, size, at, color, text.data());
}

} // namespace

void draw(ImDrawList *list, Icon icon, ImVec2 min, ImVec2 max, ImU32 color)
{
    if (icon == Icon::Diamond) {
        drawDiamond(list, min, max, color);
        return;
    }
    if (icon == Icon::Folder) {
        drawFolder(list, min, max, color);
        return;
    }
    if (icon == Icon::Slice) {
        drawSliceIcon(list, min, max, color);
        return;
    }
    drawGlyph(list, icon, min, max, color, glyph_size(theme::scale()));
}

bool round_button(Icon icon, const char *id, const char *tooltip, float diameter, bool enabled)
{
    ImGui::BeginDisabled(!enabled);
    bool clicked = ImGui::InvisibleButton(id, ImVec2(diameter, diameter));
    ImGui::EndDisabled();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImGuiCol fill = ImGuiCol_Button;
    if (ImGui::IsItemActive()) {
        fill = ImGuiCol_ButtonActive;
    } else if (ImGui::IsItemHovered()) {
        fill = ImGuiCol_ButtonHovered;
    }
    ImDrawList *list = ImGui::GetWindowDrawList();
    list->AddCircleFilled(ImVec2((min.x + max.x) / 2.0f, (min.y + max.y) / 2.0f), diameter / 2.0f,
                          ImGui::GetColorU32(fill), 0);
    drawGlyph(list, icon, min, max,
              enabled ? withAlpha(theme::INK, 1.0f) : withAlpha(theme::MUTE, 0.45f),
              round_glyph_size(theme::scale()));
    theme::hint(tooltip, enabled);
    return clicked && enabled;
}

bool button(Icon icon, const char *id, const char *tooltip, ImVec2 size, bool enabled)
{
    ImGui::BeginDisabled(!enabled);
    bool clicked = ImGui::Button(id, size);
    ImGui::EndDisabled();
    draw(ImGui::GetWindowDrawList(), icon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
         enabled ? withAlpha(theme::INK, 1.0f) : withAlpha(theme::MUTE, 0.45f));
    theme::hint(tooltip, enabled);
    return clicked && enabled;
}

bool choice(Icon icon, const char *id, const char *tooltip, bool selected, ImVec2 size)
{
    bool clicked = ImGui::InvisibleButton(id, size);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    /* The fill eases between the resting, hovered and selected looks instead of snapping. */
    const ImGuiID key = ImGui::GetItemID();
    float hover = theme::animate(key, ImGui::IsItemHovered() ? 1.0f : 0.0f);
    float pick = theme::animate(key + 1, selected ? 1.0f : 0.0f);
    const bool active = ImGui::IsItemActive();
    ImDrawList *list = ImGui::GetWindowDrawList();
    const float round = ImGui::GetStyle().FrameRounding;
    list->AddRectFilled(min, max,
                        active ? withAlpha(theme::PRESSED, 1.0f)
                               : theme::mix(theme::CANVAS_SOFT, theme::HOVER, hover),
                        round);
    list->AddRectFilled(min, max, withAlpha(active ? theme::ACCENT_STRONG : theme::ACCENT, pick),
                        round);
    draw(list, icon, min, max, withAlpha(selected ? theme::ACCENT_TEXT : theme::INK, 1.0f));
    theme::tip(tooltip);
    return clicked;
}

bool toggle(Icon on, Icon off, const char *id, const char *tooltip, bool state, float size)
{
    bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size));
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList *list = ImGui::GetWindowDrawList();
    float hover = theme::animate(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.0f : 0.0f);
    if (hover > 0.0f) {
        list->AddRectFilled(min, max, withAlpha(theme::HOVER, hover), ROUND);
    }
    draw(list, state ? on : off, min, max,
         state ? withAlpha(theme::INK, 1.0f) : withAlpha(theme::MUTE, 0.7f));
    theme::tip(tooltip);
    return clicked;
}

} // namespace icons
