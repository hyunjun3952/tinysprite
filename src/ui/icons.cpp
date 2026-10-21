#include "icons.h"

#include <algorithm>
#include <cmath>

#include "imgui.h"

namespace
{

#include "icon_font.inc"

} // namespace

namespace icons
{

namespace
{

/* UTF-8 of the Material Symbols code point for each icon, in Icon order (see
 * scripts/gen-icon-font.py). */
const char *const GLYPHS[] = {
    "\xEF\x82\x97", /* edit U+F097 */
    "\xEE\x9B\x90", /* ink_eraser U+E6D0 */
    "\xEE\x88\xBA", /* format_color_fill U+E23A */
    "\xEE\x8E\xB8", /* colorize U+E3B8 */
    "\xEF\x90\x9E", /* diagonal_line U+F41E */
    "\xEE\x8F\x86", /* crop_square U+E3C6 */
    "\xEE\xBD\x8A", /* circle U+EF4A */
    "\xEE\xBD\x92", /* highlight_alt U+EF52 */
    "\xEE\x8F\xA8", /* flip U+E3E8 */
    "\xEE\x80\x80", /* flip rotated, U+E000 */
    "\xEE\x90\x9A", /* rotate_right U+E41A */
    "\xEE\x90\x99", /* rotate_left U+E419 */
    "\xEE\xA3\x94", /* swap_horiz U+E8D4 */
    "\xEE\x85\x85", /* add U+E145 */
    "\xEE\x85\x9B", /* remove U+E15B */
    "\xEE\x85\x8D", /* content_copy U+E14D */
    "\xEE\x89\x98", /* vertical_align_bottom U+E258 */
    "\xEE\x97\x98", /* arrow_upward U+E5D8 */
    "\xEE\x97\x9B", /* arrow_downward U+E5DB */
    "\xEE\xA2\x9E", /* open_in_new U+E89E */
};
static_assert(sizeof(GLYPHS) / sizeof(GLYPHS[0]) == static_cast<size_t>(Icon::PopOut) + 1,
              "one glyph per Icon");

/* Material Symbols are drawn on a 24-unit grid with 2-unit strokes, so every stroke is a whole
 * number of pixels only when the em is a multiple of 12 px (24 px is crisp, 20 px is a little
 * softer; a 16 px glyph smears most edges). 24 px glyphs looked too big in these buttons, so a
 * button's glyph fills BUTTON_GLYPH_FILL of it, but never more than BUTTON_GLYPH_MAX_EM. */
constexpr float BUTTON_GLYPH_FILL = 0.72f;
constexpr float BUTTON_GLYPH_MAX_EM = 20.0f;
constexpr float TITLE_GLYPH_EM = 12.0f;

/* The size the font is first baked at; ImGui bakes other sizes on demand. */
constexpr float ICON_FONT_PIXELS = 24.0f;

/* Like ImGui's own context and font atlas, which it lives in, the icon font is process-wide. */
ImFont *iconFont = nullptr;

/* The em of a glyph in a button of side `size`: even, so it leaves the same whole margin on both
 * sides in the even-sized buttons the UI uses. */
float buttonGlyphEm(float size)
{
    return std::min(BUTTON_GLYPH_MAX_EM, 2.0f * std::floor(size * BUTTON_GLYPH_FILL / 2.0f));
}

/* Draws the icon's glyph at `em` pixels, centered in the square at `pos` of side `size`. */
void drawGlyph(ImDrawList *draw, Icon icon, ImVec2 pos, float size, float em, ImU32 color)
{
    if (!iconFont) {
        return;
    }
    /* The font's line box is exactly its em (see scripts/gen-icon-font.py), so text of size S fills
     * an S x S square and centering is plain arithmetic. ImGui snaps the text position to whole
     * pixels, so round it here; an odd difference between `size` and `em` still leaves the glyph
     * half a pixel off-center, which is not worth changing the glyph size for. */
    ImVec2 origin(std::round(pos.x + (size - em) / 2.0f), std::round(pos.y + (size - em) / 2.0f));
    draw->AddText(iconFont, em, origin, color, GLYPHS[static_cast<size_t>(icon)]);
}

} // namespace

bool init()
{
    ImFontAtlas *fonts = ImGui::GetIO().Fonts;
    /* The first font added becomes the default, so register ImGui's own before the icon font. */
    fonts->AddFontDefault();
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false; /* the data is a static array */
    /* Oversampling softens a glyph and shifts it about half a pixel sideways. Icons are drawn at
     * sizes that put their strokes on whole pixels, so it would only blur them. */
    config.OversampleH = 1;
    iconFont = fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(ICON_FONT_DATA),
                                           ICON_FONT_SIZE, ICON_FONT_PIXELS, &config);
    return iconFont != nullptr;
}

bool titleButton(Icon icon, const char *tooltip, float rightInset)
{
    ImVec2 window = ImGui::GetWindowPos();
    float bar = ImGui::GetFrameHeight();
    float size = bar - 4.0f;
    ImVec2 pos(window.x + ImGui::GetWindowWidth() - ImGui::GetStyle().FramePadding.x - rightInset -
                   size,
               window.y + 2.0f);
    ImVec2 end(pos.x + size, pos.y + size);
    bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(pos, end, false);
    ImDrawList *draw = ImGui::GetWindowDrawList();
    /* The title bar lies outside the window's content clip rect. */
    draw->PushClipRect(window, ImVec2(window.x + ImGui::GetWindowWidth(), window.y + bar), false);
    if (hovered) {
        draw->AddRectFilled(pos, end, IM_COL32(255, 255, 255, 45), 3.0f);
    }
    drawGlyph(draw, icon, pos, size, TITLE_GLYPH_EM, IM_COL32(255, 255, 255, 255));
    draw->PopClipRect();
    if (hovered && tooltip) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left, false);
}

bool button(Icon icon, const char *id, const char *tooltip, bool selected, float size)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size));
    bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    bool held = ImGui::IsItemActive();
    ImDrawList *draw = ImGui::GetWindowDrawList();
    float alpha = ImGui::GetStyle().Alpha;
    int a = static_cast<int>(255.0f * alpha);

    ImU32 background = 0;
    if (selected) {
        background = IM_COL32(70, 100, 150, a);
    } else if (held) {
        background = IM_COL32(80, 80, 86, a);
    } else if (hovered) {
        background = IM_COL32(62, 62, 68, a);
    }
    ImVec2 end(pos.x + size, pos.y + size);
    if (background != 0) {
        draw->AddRectFilled(pos, end, background, 3.0f);
    }
    draw->AddRect(pos, end, IM_COL32(75, 75, 82, a), 3.0f, 0, 1.0f);

    int shade = selected || hovered ? 240 : 170;
    drawGlyph(draw, icon, pos, size, buttonGlyphEm(size), IM_COL32(shade, shade, shade + 4, a));

    if (hovered && tooltip) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return clicked;
}

} // namespace icons
