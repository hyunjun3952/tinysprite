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
    0xe5ce, /* ChevronUp: expand_less */
    0xe5cf, /* ChevronDown: expand_more */
};

constexpr float ROUND = 6.0f;
constexpr float GLYPH_SIZE_BASE = 20.0f;

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

float glyph_size(float uiScale)
{
    return std::round(GLYPH_SIZE_BASE * uiScale);
}

ImVec2 glyph_origin(ImVec2 min, ImVec2 max, float x0, float y0, float x1, float y1)
{
    return ImVec2(std::floor((min.x + max.x) / 2.0f - (x0 + x1) / 2.0f),
                  std::floor((min.y + max.y) / 2.0f - (y0 + y1) / 2.0f));
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

void draw(ImDrawList *list, Icon icon, ImVec2 min, ImVec2 max, ImU32 color)
{
    if (icon == Icon::Diamond) {
        drawDiamond(list, min, max, color);
        return;
    }
    ImFont *font = ImGui::GetFont();
    const float size = glyph_size(theme::scale());
    const ImFontGlyph *glyph =
        font->GetFontBaked(size)->FindGlyph(static_cast<ImWchar>(codepoint(icon)));
    if (glyph == nullptr || !glyph->Visible) {
        return;
    }
    ImVec2 at = glyph_origin(min, max, glyph->X0, glyph->Y0, glyph->X1, glyph->Y1);
    const std::array<char, 4> text = utf8(codepoint(icon));
    list->AddText(font, size, at, color, text.data());
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
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, theme::rgb(theme::ACCENT));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::rgb(theme::ACCENT));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::rgb(theme::ACCENT_STRONG));
    }
    bool clicked = ImGui::Button(id, size);
    if (selected) {
        ImGui::PopStyleColor(3);
    }
    draw(ImGui::GetWindowDrawList(), icon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
         withAlpha(selected ? theme::ACCENT_TEXT : theme::INK, 1.0f));
    theme::tip(tooltip);
    return clicked;
}

bool toggle(Icon on, Icon off, const char *id, const char *tooltip, bool state, float size)
{
    bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size));
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList *list = ImGui::GetWindowDrawList();
    if (ImGui::IsItemHovered()) {
        list->AddRectFilled(min, max, withAlpha(theme::HOVER, 1.0f), ROUND);
    }
    draw(list, state ? on : off, min, max,
         state ? withAlpha(theme::INK, 1.0f) : withAlpha(theme::MUTE, 0.7f));
    theme::tip(tooltip);
    return clicked;
}

} // namespace icons
