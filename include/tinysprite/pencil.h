#ifndef TINYSPRITE_PENCIL_H
#define TINYSPRITE_PENCIL_H

#include "tinysprite/history.h"

typedef struct ToolPoint {
    int x, y;
} ToolPoint;

/* Draws one stroke through the points: a single point plots a pixel, several points are joined by
 * straight lines. Pixels outside the cel are skipped. Returns 0 on success (including strokes
 * that changed nothing), -1 on invalid arguments, an active stroke, or allocation failure. */
int pencil_stroke(History *history, Cel *cel, const ToolPoint *points, int count, Rgba color);

/* Same as pencil_stroke with fully transparent pixels. */
int eraser_stroke(History *history, Cel *cel, const ToolPoint *points, int count);

/* A stroke built incrementally while the mouse is dragging. It owns the history's active stroke
 * from pencil_stroke_begin until pencil_stroke_end / pencil_stroke_cancel. */
typedef struct PencilStroke PencilStroke;

/* Starts a stroke at (x, y), plotting that pixel; `erase` plots fully transparent pixels instead
 * of `color`. Returns NULL on invalid arguments, an active stroke, or allocation failure. */
PencilStroke *pencil_stroke_begin(History *history, Cel *cel, Rgba color, int erase, int x, int y);

/* Joins the previous point to (x, y) with a line. Pixels outside the cel are skipped. Returns 0 on
 * success, -1 if stroke is NULL. */
int pencil_stroke_extend(PencilStroke *stroke, int x, int y);

/* Commits the stroke as one undo step and frees it. Returns the history_end_stroke result. */
int pencil_stroke_end(PencilStroke *stroke);

/* Reverts the stroke's pixels, records no undo step, and frees it. */
void pencil_stroke_cancel(PencilStroke *stroke);

#endif
