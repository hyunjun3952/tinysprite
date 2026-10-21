#ifndef TINYSPRITE_LAYER_OPS_H
#define TINYSPRITE_LAYER_OPS_H

#include <stdint.h>

#include "tinysprite/history.h"
#include "tinysprite/sprite.h"

/* Undoable layer edits: each call applies the change and records it as one undo step. All of them
 * fail without changing anything while a stroke is active. A layer removed (or added and then
 * undone) stays alive inside the history, so its cels and the pixel steps recorded on them remain
 * valid; the history frees it when the step is dropped. A property op that would not change the
 * value returns 0 and records nothing. */

/* Appends a layer on top. Returns it, or NULL on a stroke in progress or allocation failure. */
Layer *layer_op_add(History *history, Sprite *sprite, const char *name);

/* Returns 0 on success, -1 if index is out of range, only one layer is left, a stroke is active,
 * or on allocation failure. */
int layer_op_remove(History *history, Sprite *sprite, int index);

/* Moves the layer at `from` to index `to`. Returns 0 on success (including from == to), -1 if
 * either index is out of range, a stroke is active, or on allocation failure. */
int layer_op_move(History *history, Sprite *sprite, int from, int to);

int layer_op_set_name(History *history, Layer *layer, const char *name);
int layer_op_set_visible(History *history, Layer *layer, int visible);
int layer_op_set_opacity(History *history, Layer *layer, uint8_t opacity);

#endif
