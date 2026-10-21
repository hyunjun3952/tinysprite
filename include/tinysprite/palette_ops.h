#ifndef TINYSPRITE_PALETTE_OPS_H
#define TINYSPRITE_PALETTE_OPS_H

#include "tinysprite/history.h"
#include "tinysprite/palette.h"
#include "tinysprite/sprite.h"

/* Replaces the sprite's palette as one undo step (an added, removed, imported or reset palette all
 * come down to this). Setting the palette it already has returns true and records nothing; the
 * change is refused (false) while a stroke is active. */
bool palette_op_set(History &history, Sprite &sprite, Palette next);

#endif
