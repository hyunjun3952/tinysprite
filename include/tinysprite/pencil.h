#ifndef TINYSPRITE_PENCIL_H
#define TINYSPRITE_PENCIL_H

#include "tinysprite/history.h"
#include "tinysprite/symmetry.h"

/* Draws one stroke through the points: a single point plots a pixel, several points are joined by
 * straight lines. Pixels outside the cel are skipped. Returns 0 on success (including strokes
 * that changed nothing), -1 on invalid arguments, an active stroke, or allocation failure. */
int pencil_stroke(History *history, Cel *cel, const ToolPoint *points, int count, Rgba color);

/* Same as pencil_stroke with fully transparent pixels. */
int eraser_stroke(History *history, Cel *cel, const ToolPoint *points, int count);

/* A stroke built incrementally while the mouse is dragging. It owns the history's active stroke
 * from pencil_stroke_begin until pencil_stroke_end / pencil_stroke_cancel. */
typedef struct PencilStroke PencilStroke;

#define PENCIL_BRUSH_MAX 64

/* Options for live strokes. Symmetry mirrors every pixel across the cel's center line(s); a
 * mirrored copy that falls outside the cel is clipped. Pixel-perfect drops the middle pixel of
 * every L-shaped corner so diagonal runs stay one pixel wide; it only applies to a one-pixel
 * brush. `size` is the brush width in pixels (values below 1 mean 1, above PENCIL_BRUSH_MAX are
 * capped): the brush covers size x size pixels around each plotted point, an even size reaching
 * one pixel further right and down, with the corners cut off when `round` is set. All fields
 * default to 0 (off, one-pixel square brush). */
typedef struct PencilOptions {
    bool pixel_perfect;
    Symmetry symmetry;
    int size;
    bool round;
} PencilOptions;

/* The brush a PencilOptions describes. pencil_brush_size is the effective width (1..
 * PENCIL_BRUSH_MAX); the brush covers the pixels (i, j), 0 <= i, j < size, for which
 * pencil_brush_contains is true, laid out so that plotting (x, y) paints (x - origin + i,
 * y - origin + j). */
int pencil_brush_size(const PencilOptions *options);
int pencil_brush_origin(const PencilOptions *options);
bool pencil_brush_contains(const PencilOptions *options, int i, int j);

/* Starts a stroke at (x, y), plotting that pixel in `color` (pass transparent to erase). Returns
 * NULL on invalid arguments, an active stroke, or allocation failure. */
PencilStroke *pencil_stroke_begin(History *history, Cel *cel, Rgba color, PencilOptions options,
                                  int x, int y);

/* Joins the previous point to (x, y) with a line. Pixels outside the cel are skipped. Returns 0 on
 * success, -1 if stroke is NULL or an allocation failed (now or earlier in the stroke: the stroke
 * can then only be ended, which cancels it, or cancelled). */
int pencil_stroke_extend(PencilStroke *stroke, int x, int y);

/* Commits the stroke as one undo step and frees it. Returns the history_end_stroke result, or -1
 * (with the stroke reverted) if an allocation failed while drawing it. */
int pencil_stroke_end(PencilStroke *stroke);

/* Reverts the stroke's pixels, records no undo step, and frees it. */
void pencil_stroke_cancel(PencilStroke *stroke);

#endif
