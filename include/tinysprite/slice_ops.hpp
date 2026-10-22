#ifndef TINYSPRITE_SLICE_OPS_HPP
#define TINYSPRITE_SLICE_OPS_HPP

#include "tinysprite/history.hpp"
#include "tinysprite/sprite.hpp"

/* Undoable slice edits: each call applies the change and records it as one undo step. All of them
 * fail without changing anything for an invalid slice or index, or while a stroke is active.
 * Setting a slice to what it already is returns true and records nothing. */

bool slice_op_add(History &history, Sprite &sprite, Sprite::Slice slice);
bool slice_op_set(History &history, Sprite &sprite, int index, Sprite::Slice slice);
bool slice_op_remove(History &history, Sprite &sprite, int index);

#endif
