#include "icons.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "icon_font.h"

#define COL32(r, g, b, a)                                                                          \
    ((ImU32)(((ImU32)(a) << 24) | ((ImU32)(b) << 16) | ((ImU32)(g) << 8) | (ImU32)(r)))

/* UTF-8 of the Material Symbols code point for each icon (see scripts/gen-icon-font.py). */
static const char *const GLYPHS[] = {
    [ICON_PENCIL] = "\xEF\x82\x97",     /* edit U+F097 */
    [ICON_ERASER] = "\xEE\x9B\x90",     /* ink_eraser U+E6D0 */
    [ICON_BUCKET] = "\xEE\x88\xBA",     /* format_color_fill U+E23A */
    [ICON_EYEDROPPER] = "\xEE\x8E\xB8", /* colorize U+E3B8 */
    [ICON_LINE] = "\xEF\x90\x9E",       /* diagonal_line U+F41E */
    [ICON_RECT] = "\xEE\x8F\x86",       /* crop_square U+E3C6 */
    [ICON_ELLIPSE] = "\xEE\xBD\x8A",    /* circle U+EF4A */
    [ICON_SELECT] = "\xEE\xBD\x92",     /* highlight_alt U+EF52 */
    [ICON_FLIP_H] = "\xEE\x8F\xA8",     /* flip U+E3E8 */
    [ICON_FLIP_V] = "\xEE\x80\x80",     /* flip rotated, U+E000 */
    [ICON_ROTATE_CW] = "\xEE\x90\x9A",  /* rotate_right U+E41A */
    [ICON_ROTATE_CCW] = "\xEE\x90\x99", /* rotate_left U+E419 */
    [ICON_SWAP] = "\xEE\xA3\x94",       /* swap_horiz U+E8D4 */
    [ICON_ADD] = "\xEE\x85\x85",        /* add U+E145 */
    [ICON_REMOVE] = "\xEE\x85\x9B",     /* remove U+E15B */
    [ICON_DUPLICATE] = "\xEE\x85\x8D",  /* content_copy U+E14D */
    [ICON_MERGE_DOWN] = "\xEE\x89\x98", /* vertical_align_bottom U+E258 */
    [ICON_MOVE_UP] = "\xEE\x97\x98",    /* arrow_upward U+E5D8 */
    [ICON_MOVE_DOWN] = "\xEE\x97\x9B",  /* arrow_downward U+E5DB */
    [ICON_POP_OUT] = "\xEE\xA2\x9E",    /* open_in_new U+E89E */
};

#define GLYPH_SHIFT_X 0.13f
#define GLYPH_SHIFT_X_SMALL 0.15f

#define SMALL_ICON_LIMIT 36.0f /* glyph pixel snapping differs below this button size */

static ImFont *icon_font;

int icons_init(void)
{
    /* The first font added becomes the default, so register ImGui's own before the icon font. */
    ImFontAtlas_AddFontDefault(igGetIO_Nil()->Fonts, NULL);
    ImFontConfig *config = ImFontConfig_ImFontConfig();
    config->FontDataOwnedByAtlas = false; /* the data is a static array */
    icon_font = ImFontAtlas_AddFontFromMemoryTTF(igGetIO_Nil()->Fonts, (void *)ICON_FONT_DATA,
                                                 ICON_FONT_SIZE, 24.0f, config, NULL);
    ImFontConfig_destroy(config);
    return icon_font != NULL;
}

/* Draws the icon's glyph centered in the square at `pos` of side `size`. */
static void draw_glyph(ImDrawList *draw, IconKind icon, ImVec2 pos, float size, float glyph_size,
                       ImU32 color)
{
    if (!icon_font) {
        return;
    }
    /* The glyphs sit about a tenth of their size left of, and a bit above, the center of their em
     * box; measured from rendered output. */
    float inset_x = (size - glyph_size) / 2.0f +
                    glyph_size * (size < SMALL_ICON_LIMIT ? GLYPH_SHIFT_X_SMALL : GLYPH_SHIFT_X);
    float inset_y = (size - glyph_size) / 2.0f + (size < SMALL_ICON_LIMIT ? 1.25f : 0.2f);
    ImDrawList_AddText_FontPtr(draw, icon_font, glyph_size,
                               (ImVec2){pos.x + inset_x, pos.y + inset_y}, color, GLYPHS[icon],
                               NULL, 0.0f, NULL);
}

int icon_title_button(IconKind icon, const char *tooltip, float right_inset)
{
    ImVec2 window = igGetWindowPos();
    float bar = igGetFrameHeight();
    float size = bar - 4.0f;
    ImVec2 pos = {window.x + igGetWindowWidth() - igGetStyle()->FramePadding.x - right_inset - size,
                  window.y + 2.0f};
    ImVec2 end = {pos.x + size, pos.y + size};
    int hovered = igIsWindowHovered(0) && igIsMouseHoveringRect(pos, end, false);
    ImDrawList *draw = igGetWindowDrawList();
    /* The title bar lies outside the window's content clip rect. */
    ImDrawList_PushClipRect(draw, window, (ImVec2){window.x + igGetWindowWidth(), window.y + bar},
                            false);
    if (hovered) {
        ImDrawList_AddRectFilled(draw, pos, end, COL32(255, 255, 255, 45), 3.0f, 0);
    }
    draw_glyph(draw, icon, pos, size, size * 1.05f, COL32(255, 255, 255, 255));
    ImDrawList_PopClipRect(draw);
    if (hovered && tooltip) {
        igSetTooltip("%s", tooltip);
    }
    return hovered && igIsMouseClicked_Bool(ImGuiMouseButton_Left, false);
}

int icon_button(IconKind icon, const char *id, const char *tooltip, int selected, float size)
{
    ImGuiStyle *style = igGetStyle();
    ImVec2 pos = igGetCursorScreenPos();
    int clicked = igInvisibleButton(id, (ImVec2){size, size}, 0);
    int hovered = igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    int held = igIsItemActive();
    ImDrawList *draw = igGetWindowDrawList();
    float alpha = style->Alpha;

    ImU32 background = 0;
    if (selected) {
        background = COL32(70, 100, 150, (int)(255.0f * alpha));
    } else if (held) {
        background = COL32(80, 80, 86, (int)(255.0f * alpha));
    } else if (hovered) {
        background = COL32(62, 62, 68, (int)(255.0f * alpha));
    }
    ImVec2 end = {pos.x + size, pos.y + size};
    if (background != 0) {
        ImDrawList_AddRectFilled(draw, pos, end, background, 3.0f, 0);
    }
    ImDrawList_AddRect(draw, pos, end, COL32(75, 75, 82, (int)(255.0f * alpha)), 3.0f, 1.0f, 0);

    int shade = selected || hovered ? 240 : 170;
    draw_glyph(draw, icon, pos, size, size * 0.7f,
               COL32(shade, shade, shade + 4, (int)(255.0f * alpha)));

    if (hovered && tooltip) {
        igSetTooltip("%s", tooltip);
    }
    return clicked;
}
