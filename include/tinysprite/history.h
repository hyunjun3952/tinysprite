#ifndef TINYSPRITE_HISTORY_H
#define TINYSPRITE_HISTORY_H

#include "tinysprite/cel.h"

/* Undo/redo log. One undo step is one stroke; internally a stroke is a list of pixel deltas
 * (index, old, new). Every pixel edit must go through history_set_pixel.
 * A cel must outlive every history entry that refers to it. */
typedef struct History History;

/* Returns NULL on allocation failure. */
History *history_create(void);
void history_destroy(History *history);

/* Starts a stroke on `cel`. Returns 0 on success, -1 if a stroke is already active or on
 * allocation failure. */
int history_begin_stroke(History *history, Cel *cel);

/* Writes a pixel into the active stroke's cel and records the delta. Writing the color a pixel
 * already has records nothing. Returns 0 on success, -1 if no stroke is active, (x, y) is out of
 * bounds, or on allocation failure. */
int history_set_pixel(History *history, int x, int y, Rgba color);

/* Commits the active stroke as one undo step and discards the redo steps. A stroke that changed
 * nothing is dropped. Returns 0 on success, -1 if no stroke is active or on allocation failure
 * (the stroke is then rolled back). */
int history_end_stroke(History *history);

/* Reverts every pixel written by the active stroke and discards it without touching the undo/redo
 * steps. Returns 0 on success, -1 if no stroke is active. */
int history_cancel_stroke(History *history);

int history_can_undo(const History *history);
int history_can_redo(const History *history);

/* Return 0 on success, -1 if there is nothing to undo/redo or a stroke is active. */
int history_undo(History *history);
int history_redo(History *history);

#endif
