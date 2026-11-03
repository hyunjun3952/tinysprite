#ifndef TINYSPRITE_GUIDE_OPS_HPP
#define TINYSPRITE_GUIDE_OPS_HPP

#include "tinysprite/guides.hpp"
#include "tinysprite/history.hpp"
#include "tinysprite/sprite.hpp"

/*
 * Undoable guide edits: replaces the guides of `sprite` with `next` and records the change of the
 * whole set as one undo step (a drag from a ruler, a move, a delete or "Clear guides" is one call
 * made when the gesture ends). Fails without changing anything while a stroke is active; setting
 * the guides to what they already are returns true and records nothing.
 */
bool guide_op_set(History &history, Sprite &sprite, const Guides &next);

#endif
