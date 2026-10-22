#ifndef TINYSPRITE_PALETTE_OPS_HPP
#define TINYSPRITE_PALETTE_OPS_HPP

#include "tinysprite/history.hpp"
#include "tinysprite/palette.hpp"
#include "tinysprite/sprite.hpp"

/* Replaces the sprite's palette as one undo step (an added, removed, imported or reset palette all
 * come down to this). Setting the palette it already has returns true and records nothing; the
 * change is refused (false) while a stroke is active. */
bool palette_op_set(History &history, Sprite &sprite, Palette next);

#endif
