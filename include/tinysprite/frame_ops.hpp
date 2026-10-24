#ifndef TINYSPRITE_FRAME_OPS_HPP
#define TINYSPRITE_FRAME_OPS_HPP

#include "tinysprite/history.hpp"
#include "tinysprite/sprite.hpp"

/* Undoable frame edits: each call applies the change and records it as one undo step. All of them
 * fail without changing anything while a stroke is active. A frame that is removed (or added and
 * then undone) stays alive inside the history, so the pixel steps recorded on its cels remain
 * valid; the history frees it when the step is dropped. */

/* Inserts a blank frame so it ends up at `index` (0..frameCount). Returns false for another index
 * or when the sprite is at its frame limit or cel budget (Sprite::fitsCels). */
bool frame_op_add(History &history, Sprite &sprite, int index);

/* Inserts a copy of the frame at `index` (pixels of every layer, and its duration) just after it.
 * Returns false for an index out of range or when the sprite is at its frame limit or cel budget.
 */
bool frame_op_duplicate(History &history, Sprite &sprite, int index);

/* Returns false if index is out of range or only one frame is left. */
bool frame_op_remove(History &history, Sprite &sprite, int index);

/* Moves the frame at `from` to index `to`. Returns true on success (including from == to). */
bool frame_op_move(History &history, Sprite &sprite, int from, int to);

/* ms is 1..FRAME_DURATION_MAX. Setting the value the frame already has returns true and records
 * nothing. */
bool frame_op_set_duration(History &history, Sprite &sprite, int frame, int ms);

#endif
