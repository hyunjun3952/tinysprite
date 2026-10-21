#ifndef TINYSPRITE_UI_ICONS_H
#define TINYSPRITE_UI_ICONS_H

#include <array>

#include "imgui.h"

/* Pixel-art icons, drawn from 16x16 bitmaps kept in icons.cpp (an 'X' is a lit pixel). They are
 * used only where a picture reads faster than a word: the layer rows and the layer buttons. No font
 * or generator is involved: to change an icon, edit its rows. */
namespace icons
{

enum class Icon { Eye, EyeOff, Lock, Unlock, Plus, Copy, Trash, Merge, Count };

inline constexpr int SIZE = 16; /* the bitmaps' width and height */

/* The bitmap of `icon`: SIZE rows of SIZE characters, 'X' for a lit pixel and '.' for a clear one.
 */
const std::array<const char *, SIZE> &bitmap(Icon icon);

/* Draws the icon centered in the rectangle [min, max], one screen pixel per bitmap pixel. */
void draw(ImDrawList *list, Icon icon, ImVec2 min, ImVec2 max, ImU32 color);

/* A framed button with the icon centered, like the other buttons. `id` must be unique in the ID
 * scope and start with "##" (there is no label). The tooltip shows also while disabled. Returns
 * true when clicked. */
bool button(Icon icon, const char *id, const char *tooltip, ImVec2 size, bool enabled = true);

/* A frameless square toggle for a list row: shows `on` in the text color or `off` muted, with a
 * hover highlight. Returns true when clicked. */
bool toggle(Icon on, Icon off, const char *id, const char *tooltip, bool state, float size);

} // namespace icons

#endif
