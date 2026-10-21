#ifndef TINYSPRITE_TAG_OPS_H
#define TINYSPRITE_TAG_OPS_H

#include "tinysprite/history.h"
#include "tinysprite/sprite.h"

/* Undoable tag edits: each call applies the change and records it as one undo step. All of them
 * fail without changing anything for an invalid tag or index, or while a stroke is active. Setting
 * a tag to what it already is returns true and records nothing. */

bool tag_op_add(History &history, Sprite &sprite, Sprite::Tag tag);
bool tag_op_set(History &history, Sprite &sprite, int index, Sprite::Tag tag);
bool tag_op_remove(History &history, Sprite &sprite, int index);

#endif
