#ifndef TINYSPRITE_UI_ICONS_HPP
#define TINYSPRITE_UI_ICONS_HPP

#include <array>

#include "imgui.h"

/* Pixel-art icons, drawn from 16x16 bitmaps kept in icons.cpp (an 'X' is a lit pixel). They are
 * used only where a picture reads faster than a word: the layer rows, the layer buttons and the
 * shape tools. No font or generator is involved: to change an icon, edit its rows. */
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
    Count
};

inline constexpr int SIZE = 16; /* the bitmaps' width and height */

/* The bitmap of `icon`: SIZE rows of SIZE characters, 'X' for a lit pixel and '.' for a clear one.
 */
const std::array<const char *, SIZE> &bitmap(Icon icon);

/* Draws the icon centered in the rectangle [min, max], one screen pixel per bitmap pixel. */
void draw(ImDrawList *list, Icon icon, ImVec2 min, ImVec2 max, ImU32 color);

/* A framed button with the icon centered, like the other buttons. `id` must be unique in the ID
 * scope and start with "##" (there is no label). While disabled it shows the "not allowed" cursor
 * instead of the tooltip. Returns true when clicked. */
bool button(Icon icon, const char *id, const char *tooltip, ImVec2 size, bool enabled = true);

/* A framed button for one choice out of several: filled while `selected`. Returns true when
 * clicked. */
bool choice(Icon icon, const char *id, const char *tooltip, bool selected, ImVec2 size);

/* A frameless square toggle for a list row: shows `on` in the text color or `off` muted, with a
 * hover highlight. Returns true when clicked. */
bool toggle(Icon on, Icon off, const char *id, const char *tooltip, bool state, float size);

} // namespace icons

#endif
