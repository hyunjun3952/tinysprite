#include "icons.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "imgui_internal.h"
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
    0,      /* Line: a path (icon_paths.cpp) */
    0,      /* Rect: a path (icon_paths.cpp) */
    0,      /* Ellipse: a path (icon_paths.cpp) */
    0,      /* Triangle: a path (icon_paths.cpp) */
    0,      /* Diamond: a path (icon_paths.cpp) */
    0,      /* Hexagon: a path (icon_paths.cpp) */
    0,      /* Star: a path (icon_paths.cpp) */
    0,      /* Cross: a path (icon_paths.cpp) */
    0xe15b, /* Minus: remove */
    0xe5d5, /* Reset: refresh */
    0xe5d8, /* ArrowUp: arrow_upward */
    0xe5db, /* ArrowDown: arrow_downward */
    0,      /* Pencil: a path (icon_paths.cpp) */
    0,      /* Eraser: a path (icon_paths.cpp) */
    0,      /* Bucket: a path (icon_paths.cpp) */
    0,      /* Eyedropper: a path (icon_paths.cpp) */
    0,      /* Select: a path (icon_paths.cpp) */
    0xe8d4, /* Swap: swap_horiz */
    0,      /* Folder: drawn with lines */
    0,      /* Slice: a path (icon_paths.cpp) */
    0,      /* Pan: a path (icon_paths.cpp) */
    0,      /* Wand: a path (icon_paths.cpp) */
    0,      /* Lasso: a path (icon_paths.cpp) */
    0,      /* ChevronLeft: drawn with lines */
    0,      /* ChevronRight: drawn with lines */
    0,      /* Replace: a path (icon_paths.cpp) */
};

/*
 * An invisible button the keyboard can reach: ImGui leaves an InvisibleButton out of Tab and arrow
 * navigation unless it is asked to include it, and these icon buttons are drawn by hand, so their
 * focus ring is drawn by hand as well (RenderNavCursor, after the picture).
 */
bool navigableButton(const char *id, ImVec2 size)
{
    return ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
}

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

float ink_fit(float inkWidth, float inkHeight, float target)
{
    const float longest = std::max(inkWidth, inkHeight);
    if (!(longest > 0.0f)) {
        return 1.0f;
    }
    return std::clamp(target / longest, INK_FIT_MIN, INK_FIT_MAX);
}

float fitted_glyph_size(ImFont *font, Icon icon, float size)
{
    const ImFontGlyph *glyph =
        font->GetFontBaked(size)->FindGlyph(static_cast<ImWchar>(codepoint(icon)));
    if (glyph == nullptr || !glyph->Visible) {
        return size;
    }
    const float target = INK_TARGET * size / GLYPH_SIZE_BASE;
    return std::round(size * ink_fit(glyph->X1 - glyph->X0, glyph->Y1 - glyph->Y0, target));
}

unsigned codepoint(Icon icon)
{
    return CODEPOINTS[static_cast<size_t>(icon)];
}

bool is_drawn(Icon icon)
{
    return icon == Icon::Folder || icon == Icon::ChevronLeft || icon == Icon::ChevronRight ||
           path(icon) != nullptr;
}

void chevron_points(Icon icon, ImVec2 min, ImVec2 max, ImVec2 (&points)[3])
{
    const float size = glyph_size(theme::scale());
    const float reach = std::round(size * 0.15f); /* from the middle of the box to the tip */
    const float rise = std::round(size * 0.3f);   /* from the tip's row to each end's row */
    const float cx = std::floor((min.x + max.x) / 2.0f) + 0.5f;
    const float cy = std::floor((min.y + max.y) / 2.0f) + 0.5f;
    const float toward = icon == Icon::ChevronRight ? 1.0f : -1.0f;
    points[0] = ImVec2(cx - toward * reach, cy - rise);
    points[1] = ImVec2(cx + toward * reach, cy);
    points[2] = ImVec2(cx - toward * reach, cy + rise);
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

namespace
{

/* The outline through `points` (closed) as dashes DASH_ON grid units long, DASH_OFF apart along
 * each side. */
void drawDashes(ImDrawList *list, const ImVec2 *points, int count, float unit, ImU32 color,
                float width)
{
    constexpr float DASH_ON = 1.6f;
    constexpr float DASH_OFF = 1.4f;
    for (int i = 0; i < count; i++) {
        const ImVec2 a = points[i];
        const ImVec2 b = points[(i + 1) % count];
        const float length = std::hypot(b.x - a.x, b.y - a.y);
        const float period = (DASH_ON + DASH_OFF) * unit;
        const int dashes = static_cast<int>(std::ceil(length / period));
        for (int d = 0; d < dashes; d++) {
            const float t = static_cast<float>(d) * period;
            const float end = std::min(t + DASH_ON * unit, length);
            const ImVec2 from(a.x + (b.x - a.x) * t / length, a.y + (b.y - a.y) * t / length);
            const ImVec2 to(a.x + (b.x - a.x) * end / length, a.y + (b.y - a.y) * end / length);
            list->AddLine(from, to, color, width);
        }
    }
}

/* Draws one stroke of a path whose grid starts at `at`, `unit` pixels to a grid unit. */
void drawStroke(ImDrawList *list, const Stroke &stroke, ImVec2 at, float unit, ImU32 color,
                float width)
{
    if (stroke.kind == StrokeKind::Circle) {
        list->AddCircle(ImVec2(at.x + stroke.xy[0] * unit, at.y + stroke.xy[1] * unit),
                        stroke.xy[2] * unit, color, 0, width);
        return;
    }
    constexpr int MAX_POINTS = 16;
    ImVec2 points[MAX_POINTS];
    const int count = std::min(stroke.count, MAX_POINTS);
    for (int i = 0; i < count; i++) {
        const size_t x = 2 * static_cast<size_t>(i);
        points[i] = ImVec2(at.x + stroke.xy[x] * unit, at.y + stroke.xy[x + 1] * unit);
    }
    switch (stroke.kind) {
    case StrokeKind::Open:
        list->AddPolyline(points, count, color, ImDrawFlags_None, width);
        return;
    case StrokeKind::Closed:
        list->AddPolyline(points, count, color, ImDrawFlags_Closed, width);
        return;
    case StrokeKind::Fill:
        list->AddConcavePolyFilled(points, count, color);
        return;
    case StrokeKind::Dashed:
        drawDashes(list, points, count, unit, color, width);
        return;
    case StrokeKind::Circle:
        return;
    }
}

/*
 * Draws `path` centered in [min, max]: its grid is PATH_GRID units of `scale` pixels, on a whole
 * pixel, and every line PATH_STROKE units wide, so the icons grow with the scale without jumps.
 * ImGui draws a line of a whole-pixel width from a baked texture, which reads heavier than the
 * geometry it uses for other widths, so the texture is off while a path is drawn: every scale
 * then gets the same edge.
 */
void drawPath(ImDrawList *list, const Path &path, ImVec2 min, ImVec2 max, ImU32 color)
{
    const float unit = theme::scale();
    const float side = PATH_GRID * unit;
    const ImVec2 at(std::floor((min.x + max.x - side) / 2.0f),
                    std::floor((min.y + max.y - side) / 2.0f));
    const ImDrawListFlags flags = list->Flags;
    list->Flags &= ~ImDrawListFlags_AntiAliasedLinesUseTex;
    for (int s = 0; s < path.count; s++) {
        drawStroke(list, path.strokes[s], at, unit, color, path_stroke(unit));
    }
    list->Flags = flags;
}

/* The glyph of `icon` at about `size` pixels (see fitted_glyph_size), centered in [min, max] by its
 * ink. */
void drawGlyph(ImDrawList *list, Icon icon, ImVec2 min, ImVec2 max, ImU32 color, float size)
{
    ImFont *font = ImGui::GetFont();
    size = fitted_glyph_size(font, icon, size);
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
    if (const Path *shape = path(icon)) {
        drawPath(list, *shape, min, max, color);
        return;
    }
    if (icon == Icon::Folder) {
        drawFolder(list, min, max, color);
        return;
    }
    if (icon == Icon::ChevronLeft || icon == Icon::ChevronRight) {
        ImVec2 points[3];
        chevron_points(icon, min, max, points);
        list->AddPolyline(points, 3, color, ImDrawFlags_None,
                          std::max(1.0f, std::round(glyph_size(theme::scale()) * 0.1f)));
        return;
    }
    drawGlyph(list, icon, min, max, color, glyph_size(theme::scale()));
}

ImU32 disabled_color()
{
    return withAlpha(theme::MUTE, 1.0f);
}

bool round_button(Icon icon, const char *id, const char *tooltip, float diameter, bool enabled)
{
    ImGui::BeginDisabled(!enabled);
    bool clicked = navigableButton(id, ImVec2(diameter, diameter));
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
    drawGlyph(list, icon, min, max, enabled ? withAlpha(theme::INK, 1.0f) : disabled_color(),
              round_glyph_size(theme::scale()));
    ImGui::RenderNavCursor(ImRect(min, max), ImGui::GetItemID());
    theme::hint(tooltip, enabled);
    return clicked && enabled;
}

bool button(Icon icon, const char *id, const char *tooltip, ImVec2 size, bool enabled,
            const char *reason)
{
    ImGui::BeginDisabled(!enabled);
    bool clicked = ImGui::Button(id, size);
    ImGui::EndDisabled();
    draw(ImGui::GetWindowDrawList(), icon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
         enabled ? withAlpha(theme::INK, 1.0f) : disabled_color());
    theme::hint(tooltip, enabled, reason);
    return clicked && enabled;
}

bool choice(Icon icon, const char *id, const char *tooltip, bool selected, ImVec2 size)
{
    bool clicked = navigableButton(id, size);
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
    ImGui::RenderNavCursor(ImRect(min, max), key);
    theme::tip(tooltip);
    return clicked;
}

bool toggle(Icon on, Icon off, const char *id, const char *tooltip, bool state, float size)
{
    bool clicked = navigableButton(id, ImVec2(size, size));
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList *list = ImGui::GetWindowDrawList();
    float hover = theme::animate(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.0f : 0.0f);
    if (hover > 0.0f) {
        list->AddRectFilled(min, max, withAlpha(theme::HOVER, hover), ROUND);
    }
    draw(list, state ? on : off, min, max,
         state ? withAlpha(theme::INK, 1.0f) : withAlpha(theme::MUTE, 0.7f));
    ImGui::RenderNavCursor(ImRect(min, max), ImGui::GetItemID());
    theme::tip(tooltip);
    return clicked;
}

} // namespace icons
