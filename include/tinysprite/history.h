#ifndef TINYSPRITE_HISTORY_H
#define TINYSPRITE_HISTORY_H

#include "tinysprite/cel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Undo/redo log. One undo step is one stroke; internally a stroke is a list of pixel deltas
 * (index, old, new). Every pixel edit must go through history_set_pixel. Edits that are not pixel
 * deltas (layer operations) are recorded as actions, see history_push_action.
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

/* True while a stroke is open (between history_begin_stroke and end / cancel). */
bool history_stroke_active(const History *history);

/* A non-pixel edit with its own undo / redo. `release` is called exactly once when the step is
 * dropped (new edit discards it from the redo side, or history_destroy) and must free `ctx`; the
 * callbacks may be NULL. Release must not touch objects other steps refer to, since steps are
 * dropped oldest-first. */
typedef struct HistoryAction {
    void (*undo)(void *ctx);
    void (*redo)(void *ctx);
    void (*release)(void *ctx);
    void *ctx;
} HistoryAction;

/* Records an action whose effect the caller has already applied, as one undo step, and discards
 * the redo steps. Returns 0 on success, -1 if a stroke is active, history is NULL, or on
 * allocation failure; the action is then released immediately. */
int history_push_action(History *history, HistoryAction action);

bool history_can_undo(const History *history);
bool history_can_redo(const History *history);

/* Return 0 on success, -1 if there is nothing to undo/redo or a stroke is active. */
int history_undo(History *history);
int history_redo(History *history);

/* Drops every undo/redo step that touches `cel` (cancelling the active stroke if it is on `cel`),
 * so the cel can be destroyed. The remaining steps keep their order and the undo/redo boundary.
 * Steps on other cels are assumed independent of the dropped ones. */
void history_forget_cel(History *history, Cel *cel);

#ifdef __cplusplus
}
#endif

#endif
