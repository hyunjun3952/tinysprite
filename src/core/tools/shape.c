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

static int draw(History *history, ShapeKind kind, int filled, Rgba color, int x0, int y0, int x1,
                int y1)
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

int shape_stroke(History *history, Cel *cel, ShapeKind kind, int filled, Rgba color, int x0, int y0,
                 int x1, int y1)
{
    if (!history || !cel || (unsigned)kind > SHAPE_ELLIPSE) {
        return -1;
    }
    if (history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    (void)draw(history, kind, filled, color, x0, y0, x1, y1);
    return history_end_stroke(history);
}

struct ShapeStroke {
    History *history;
    Cel *cel;
    ShapeKind kind;
    int filled;
    Rgba color;
    int anchor_x, anchor_y;
};

ShapeStroke *shape_stroke_begin(History *history, Cel *cel, ShapeKind kind, int filled, Rgba color,
                                int x, int y)
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
    *stroke = (ShapeStroke){history, cel, kind, filled, color, x, y};
    (void)draw(history, kind, filled, color, x, y, x, y);
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
    return draw(stroke->history, stroke->kind, stroke->filled, stroke->color, stroke->anchor_x,
                stroke->anchor_y, x, y);
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
