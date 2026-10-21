#ifndef TINYSPRITE_SHAPE_H
#define TINYSPRITE_SHAPE_H

#include "tinysprite/history.h"
#include "tinysprite/symmetry.h"

typedef enum ShapeKind {
    SHAPE_LINE,
    SHAPE_RECT,
    SHAPE_ELLIPSE,
} ShapeKind;

/* Draws the shape spanning (x0, y0)-(x1, y1) as one undo step (`filled` is ignored for lines).
 * Pixels outside the cel are skipped. Returns 0 on success, -1 on invalid arguments, an active
 * stroke, or allocation failure. */
int shape_stroke(History *history, Cel *cel, ShapeKind kind, int filled, Rgba color, int x0, int y0,
                 int x1, int y1);

/* shape_stroke with symmetry: the shape is also drawn reflected across each enabled axis, all in
 * the same undo step. Reflected parts outside the cel are clipped. */
int shape_stroke_ex(History *history, Cel *cel, ShapeKind kind, int filled, Rgba color,
                    Symmetry symmetry, int x0, int y0, int x1, int y1);

/* A shape dragged out with the mouse: it is anchored at the press point and redrawn from scratch
 * on every update, so shrinking it restores the pixels underneath. It owns the history's active
 * stroke from shape_stroke_begin until shape_stroke_end / shape_stroke_cancel. */
typedef struct ShapeStroke ShapeStroke;

/* Starts a shape anchored at (x, y), plotting that pixel. Returns NULL on invalid arguments, an
 * active stroke, or allocation failure. */
ShapeStroke *shape_stroke_begin(History *history, Cel *cel, ShapeKind kind, int filled, Rgba color,
                                int x, int y);

/* shape_stroke_begin with symmetry (see shape_stroke_ex). */
ShapeStroke *shape_stroke_begin_ex(History *history, Cel *cel, ShapeKind kind, int filled,
                                   Rgba color, Symmetry symmetry, int x, int y);

/* Replaces the preview with the shape from the anchor to (x, y). Returns 0 on success, -1 if
 * stroke is NULL or on allocation failure. */
int shape_stroke_update(ShapeStroke *stroke, int x, int y);

/* Commits the shape as one undo step and frees the stroke. Returns the history_end_stroke result.
 */
int shape_stroke_end(ShapeStroke *stroke);

/* Reverts the shape's pixels, records no undo step, and frees the stroke. */
void shape_stroke_cancel(ShapeStroke *stroke);

#endif
