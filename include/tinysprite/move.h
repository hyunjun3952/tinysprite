#ifndef TINYSPRITE_MOVE_H
#define TINYSPRITE_MOVE_H

#include "tinysprite/history.h"
#include "tinysprite/rect.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Moves the pixels of a rectangular selection: the selected area is cleared and its non-transparent
 * pixels are written at the offset, over whatever is there (transparent pixels leave the
 * destination alone). Pixels moved outside the cel are lost. The selection is clipped to the cel
 * first. Returns 0 on success, -1 on invalid arguments, an empty selection, an active stroke, or
 * allocation failure. */
int move_stroke(History *history, Cel *cel, Rect selection, int dx, int dy);

/* A selection dragged with the mouse. The pixels are captured once; every update redraws the cel
 * from the original, so earlier previews never accumulate. It owns the history's active stroke
 * from move_stroke_begin until move_stroke_end / move_stroke_cancel. */
typedef struct MoveStroke MoveStroke;

/* Returns NULL on invalid arguments, a selection with no pixels inside the cel, an active stroke,
 * or allocation failure. Nothing is drawn until the first update. */
MoveStroke *move_stroke_begin(History *history, Cel *cel, Rect selection);

/* Shows the selection displaced by (dx, dy) from where it started (not from the last update).
 * Returns 0 on success, -1 if stroke is NULL or on allocation failure (the stroke is then
 * unusable: cancel it). */
int move_stroke_update(MoveStroke *stroke, int dx, int dy);

/* Commits the move as one undo step (none if it ended where it began) and frees the stroke.
 * Returns the history_end_stroke result. */
int move_stroke_end(MoveStroke *stroke);

/* Reverts the cel, records no undo step, and frees the stroke. */
void move_stroke_cancel(MoveStroke *stroke);

#ifdef __cplusplus
}
#endif

#endif
