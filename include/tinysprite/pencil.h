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

#endif
