#ifndef TINYSPRITE_UI_ICONS_HPP
#define TINYSPRITE_UI_ICONS_HPP

#include "imgui.h"

/* Icons: Material Symbols (Outlined, Apache 2.0) glyphs from the font that theme::load_fonts()
 * merges into the UI font, so they scale with the UI, take any color and add no image files. They
 * are used where a picture reads faster than a word: the tools, the layer rows and buttons, the
 * palette buttons. To add one, add an Icon and its codepoint (the names are in the Material
 * Symbols codepoints list) and put the codepoint in the subset in thirdparty/material-symbols. */
namespace icons
{

enum class Icon {
    Eye,
    EyeOff,
    Lock,
    Unlock,
    Plus,
    Copy,
    Trash,
    Merge,
    Line,
    Rect,
    Ellipse,
    Triangle,
    Diamond,
    Hexagon,
    Star,
    Cross,
    Minus,
    Reset,
    ArrowUp,
    ArrowDown,
    Pencil,
    Eraser,
    Bucket,
    Eyedropper,
    Select,
    Swap,
    Folder,
    Slice,
    Pan,
    Wand,
    Lasso,
    ChevronLeft,
    ChevronRight,
    Count
};

/* The Material Symbols codepoint of `icon` (in the Private Use Area); 0 for the icons that are
 * drawn with lines instead (see is_drawn). */
unsigned codepoint(Icon icon);

/* Whether `icon` is drawn by this module and has no glyph: the tool and shape icons (bitmaps) and
 * the Folder and the two chevrons (lines). */
bool is_drawn(Icon icon);

/* The three points of a ChevronLeft or ChevronRight (the two strokes meet at the middle one) for
 * the box [min, max]: whole pixels, centered across and along the box. The tip points the way the
 * icon is named. */
void chevron_points(Icon icon, ImVec2 min, ImVec2 max, ImVec2 (&points)[3]);

/* The side of a tool icon bitmap, in cells. */
inline constexpr int BITMAP_SIZE = 16;

/* A tool icon as a picture on a BITMAP_SIZE square grid: row y, column x is ink when
 * rows[y][x] == '#'. */
struct Bitmap {
    const char *rows[BITMAP_SIZE];
};

/* The bitmap of `icon`, or null for an icon that is a font glyph or drawn with lines. */
const Bitmap *bitmap(Icon icon);

/* The side of one bitmap cell for a UI scale: whole pixels (at least one), so the picture keeps
 * its pixel grid and every icon of the set is the same size. */
float bitmap_cell(float uiScale);

/*
 * The nominal pixel size of an icon glyph for a UI scale: 20 px at scale 1, in whole pixels so
 * the strokes stay on the pixel grid. draw() adjusts it per icon (fitted_glyph_size).
 */
float glyph_size(float uiScale);

/* The longer side of an icon's ink at scale 1, in pixels. The font draws the bucket 17 px long and
 * the plus 9 px at the same size, so each glyph is drawn at the size that brings its ink to this
 * length (ink_fit), within INK_FIT_MIN..INK_FIT_MAX of the nominal size so a tiny glyph such as the
 * minus sign is not blown up into a heavy one. */
inline constexpr float INK_TARGET = 15.0f;
inline constexpr float INK_FIT_MIN = 0.8f;
inline constexpr float INK_FIT_MAX = 1.5f;

/* The factor to draw a glyph by, so its ink (`inkWidth` x `inkHeight` at the nominal size) gets a
 * longer side of `target`; 1 when it has no ink. */
float ink_fit(float inkWidth, float inkHeight, float target);

/* `size` for `icon` after ink_fit, in whole pixels, measured in `font`. The target scales with
 * `size` (INK_TARGET at the 20 px of scale 1). */
float fitted_glyph_size(ImFont *font, Icon icon, float size);

/* The glyph size inside a round button: 80% of glyph_size, so the glyph sits comfortably in a
 * circle. */
float round_glyph_size(float uiScale);

/* Where to start the glyph's text so that its ink box, (x0, y0)-(x1, y1) relative to that start,
 * is centered in [min, max]. A glyph's em box is not centered on its drawing (an arrow or a
 * pencil sits off to one side of it), so the ink is what has to be centered. Whole pixels. */
ImVec2 glyph_origin(ImVec2 min, ImVec2 max, float x0, float y0, float x1, float y1);

/* Draws the icon centered in the rectangle [min, max]. */
void draw(ImDrawList *list, Icon icon, ImVec2 min, ImVec2 max, ImU32 color);

/* A framed button with the icon centered, like the other buttons. `id` must be unique in the ID
 * scope and start with "##" (there is no label). While disabled it shows the "not allowed" cursor
 * instead of the tooltip, and `reason` (why it is disabled) as a tooltip when it has one. Returns
 * true when clicked. */
bool button(Icon icon, const char *id, const char *tooltip, ImVec2 size, bool enabled = true,
            const char *reason = nullptr);

/* A circular button (a filled disc in the button colors) with the icon centered, for a small
 * secondary action such as swapping the colors. `diameter` is in pixels; the rest is as button().
 */
bool round_button(Icon icon, const char *id, const char *tooltip, float diameter,
                  bool enabled = true);

/* A framed button for one choice out of several: filled with the accent while `selected`. Returns
 * true when clicked. */
bool choice(Icon icon, const char *id, const char *tooltip, bool selected, ImVec2 size);

/* A frameless square toggle for a list row: shows `on` in the text color or `off` muted, with a
 * hover highlight. Returns true when clicked. */
bool toggle(Icon on, Icon off, const char *id, const char *tooltip, bool state, float size);

} // namespace icons

#endif
