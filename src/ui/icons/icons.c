#include "icons/icons.h"

#include <math.h>

#include "icons/icon_font.h"
#include "ui_common.h"

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

/* Material Symbols are drawn on a 24-unit grid with 2-unit strokes, so every stroke is a whole
 * number of pixels only when the em is a multiple of 12 px; at any other size the strokes straddle
 * pixel boundaries and the edges come out smeared and uneven. These are the em sizes the buttons
 * draw at, regardless of how big the button around the glyph is. */
#define BUTTON_GLYPH_EM 24.0f
#define TITLE_GLYPH_EM 12.0f

/* The size the font is first baked at; ImGui bakes other sizes on demand. */
#define ICON_FONT_PIXELS 24.0f

/* Like ImGui's own context and font atlas, which it lives in, the icon font is process-wide. */
static ImFont *icon_font;

bool icons_init(void)
{
    /* The first font added becomes the default, so register ImGui's own before the icon font. */
    ImFontAtlas_AddFontDefault(igGetIO_Nil()->Fonts, NULL);
    ImFontConfig *config = ImFontConfig_ImFontConfig();
    config->FontDataOwnedByAtlas = false; /* the data is a static array */
    /* Oversampling softens a glyph and shifts it about half a pixel sideways. Icons are drawn at
     * sizes that put their strokes on whole pixels, so it would only blur them. */
    config->OversampleH = 1;
    icon_font = ImFontAtlas_AddFontFromMemoryTTF(igGetIO_Nil()->Fonts, (void *)ICON_FONT_DATA,
                                                 ICON_FONT_SIZE, ICON_FONT_PIXELS, config, NULL);
    ImFontConfig_destroy(config);
    return icon_font != NULL;
}

/* Draws the icon's glyph at `em` pixels, centered in the square at `pos` of side `size`. */
static void draw_glyph(ImDrawList *draw, IconKind icon, ImVec2 pos, float size, float em,
                       ImU32 color)
{
    if (!icon_font) {
        return;
    }
    /* The font's line box is exactly its em (see scripts/gen-icon-font.py), so text of size S fills
     * an S x S square and centering is plain arithmetic. ImGui snaps the text position to whole
     * pixels, so round it here; an odd difference between `size` and `em` still leaves the glyph
     * half a pixel off-center, which is not worth changing the glyph size for. */
    ImVec2 origin = {roundf(pos.x + (size - em) / 2.0f), roundf(pos.y + (size - em) / 2.0f)};
    ImDrawList_AddText_FontPtr(draw, icon_font, em, origin, color, GLYPHS[icon], NULL, 0.0f, NULL);
}

bool icon_title_button(IconKind icon, const char *tooltip, float right_inset)
{
    ImVec2 window = igGetWindowPos();
    float bar = igGetFrameHeight();
    float size = bar - 4.0f;
    ImVec2 pos = {window.x + igGetWindowWidth() - igGetStyle()->FramePadding.x - right_inset - size,
                  window.y + 2.0f};
    ImVec2 end = {pos.x + size, pos.y + size};
    bool hovered = igIsWindowHovered(0) && igIsMouseHoveringRect(pos, end, false);
    ImDrawList *draw = igGetWindowDrawList();
    /* The title bar lies outside the window's content clip rect. */
    ImDrawList_PushClipRect(draw, window, (ImVec2){window.x + igGetWindowWidth(), window.y + bar},
                            false);
    if (hovered) {
        ImDrawList_AddRectFilled(draw, pos, end, COL32(255, 255, 255, 45), 3.0f, 0);
    }
    draw_glyph(draw, icon, pos, size, TITLE_GLYPH_EM, COL32(255, 255, 255, 255));
    ImDrawList_PopClipRect(draw);
    if (hovered && tooltip) {
        igSetTooltip("%s", tooltip);
    }
    return hovered && igIsMouseClicked_Bool(ImGuiMouseButton_Left, false);
}

bool icon_button(IconKind icon, const char *id, const char *tooltip, bool selected, float size)
{
    ImGuiStyle *style = igGetStyle();
    ImVec2 pos = igGetCursorScreenPos();
    bool clicked = igInvisibleButton(id, (ImVec2){size, size}, 0);
    bool hovered = igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    bool held = igIsItemActive();
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
    draw_glyph(draw, icon, pos, size, BUTTON_GLYPH_EM,
               COL32(shade, shade, shade + 4, (int)(255.0f * alpha)));

    if (hovered && tooltip) {
        igSetTooltip("%s", tooltip);
    }
    return clicked;
}
