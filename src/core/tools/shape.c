#include "tinysprite/shape.h"

#include <stdlib.h>

#include "tinysprite/raster.h"

typedef struct PlotTarget {
    History *history;
    Rgba color;
} PlotTarget;

static void plot_to_history(int x, int y, void *user)
{
    const PlotTarget *target = user;
    /* Out-of-bounds pixels are rejected by history; clipping is the intended behavior. */
    (void)history_set_pixel(target->history, x, y, target->color);
}

static int draw_one(History *history, ShapeKind kind, int filled, Rgba color, int x0, int y0,
                    int x1, int y1)
{
    PlotTarget target = {history, color};
    switch (kind) {
    case SHAPE_LINE:
        raster_line(x0, y0, x1, y1, plot_to_history, &target);
        return 0;
    case SHAPE_RECT:
        raster_rect(x0, y0, x1, y1, filled, plot_to_history, &target);
        return 0;
    case SHAPE_ELLIPSE:
        raster_ellipse(x0, y0, x1, y1, filled, plot_to_history, &target);
        return 0;
    }
    return -1;
}

/* Draws the shape and its reflections. Reflecting both end points reflects the whole shape since
 * every kind is symmetric about its bounding box. */
static int draw(History *history, const Cel *cel, ShapeKind kind, int filled, Rgba color,
                Symmetry symmetry, int x0, int y0, int x1, int y1)
{
    int width = cel_width(cel);
    int height = cel_height(cel);
    for (int flip_y = 0; flip_y <= (symmetry.mirror_y != 0); flip_y++) {
        for (int flip_x = 0; flip_x <= (symmetry.mirror_x != 0); flip_x++) {
            int ax = flip_x ? width - 1 - x0 : x0;
            int bx = flip_x ? width - 1 - x1 : x1;
            int ay = flip_y ? height - 1 - y0 : y0;
            int by = flip_y ? height - 1 - y1 : y1;
            if (draw_one(history, kind, filled, color, ax, ay, bx, by) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

int shape_stroke(History *history, Cel *cel, ShapeKind kind, int filled, Rgba color, int x0, int y0,
                 int x1, int y1)
{
    return shape_stroke_ex(history, cel, kind, filled, color, (Symmetry){0}, x0, y0, x1, y1);
}

int shape_stroke_ex(History *history, Cel *cel, ShapeKind kind, int filled, Rgba color,
                    Symmetry symmetry, int x0, int y0, int x1, int y1)
{
    if (!history || !cel || (unsigned)kind > SHAPE_ELLIPSE) {
        return -1;
    }
    if (history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    (void)draw(history, cel, kind, filled, color, symmetry, x0, y0, x1, y1);
    return history_end_stroke(history);
}

struct ShapeStroke {
    History *history;
    Cel *cel;
    ShapeKind kind;
    int filled;
    Rgba color;
    Symmetry symmetry;
    int anchor_x, anchor_y;
};

ShapeStroke *shape_stroke_begin(History *history, Cel *cel, ShapeKind kind, int filled, Rgba color,
                                int x, int y)
{
    return shape_stroke_begin_ex(history, cel, kind, filled, color, (Symmetry){0}, x, y);
}

ShapeStroke *shape_stroke_begin_ex(History *history, Cel *cel, ShapeKind kind, int filled,
                                   Rgba color, Symmetry symmetry, int x, int y)
{
    if (!history || !cel || (unsigned)kind > SHAPE_ELLIPSE) {
        return NULL;
    }
    ShapeStroke *stroke = malloc(sizeof(*stroke));
    if (!stroke) {
        return NULL;
    }
    if (history_begin_stroke(history, cel) != 0) {
        free(stroke);
        return NULL;
    }
    *stroke = (ShapeStroke){history, cel, kind, filled, color, symmetry, x, y};
    (void)draw(history, cel, kind, filled, color, symmetry, x, y, x, y);
    return stroke;
}

int shape_stroke_update(ShapeStroke *stroke, int x, int y)
{
    if (!stroke) {
        return -1;
    }
    /* Reverting the open stroke and starting a new one redraws the preview from the original
     * pixels. */
    (void)history_cancel_stroke(stroke->history);
    if (history_begin_stroke(stroke->history, stroke->cel) != 0) {
        return -1;
    }
    return draw(stroke->history, stroke->cel, stroke->kind, stroke->filled, stroke->color,
                stroke->symmetry, stroke->anchor_x, stroke->anchor_y, x, y);
}

int shape_stroke_end(ShapeStroke *stroke)
{
    if (!stroke) {
        return -1;
    }
    int result = history_end_stroke(stroke->history);
    free(stroke);
    return result;
}

void shape_stroke_cancel(ShapeStroke *stroke)
{
    if (!stroke) {
        return;
    }
    (void)history_cancel_stroke(stroke->history);
    free(stroke);
}
