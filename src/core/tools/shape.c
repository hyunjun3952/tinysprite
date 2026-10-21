#include "tinysprite/shape.h"

#include <stdlib.h>

#include "history_plot.h"
#include "tinysprite/raster.h"

/* The kind arrives from callers as a plain enum value; reject anything the switch below skips. */
static bool valid_kind(ShapeKind kind)
{
    return kind == SHAPE_LINE || kind == SHAPE_RECT || kind == SHAPE_ELLIPSE;
}

static void draw_one(HistoryPlot *plot, ShapeStyle style, ToolPoint a, ToolPoint b)
{
    switch (style.kind) {
    case SHAPE_LINE:
        raster_line(a.x, a.y, b.x, b.y, history_plot, plot);
        break;
    case SHAPE_RECT:
        raster_rect(a.x, a.y, b.x, b.y, style.filled, history_plot, plot);
        break;
    case SHAPE_ELLIPSE:
        raster_ellipse(a.x, a.y, b.x, b.y, style.filled, history_plot, plot);
        break;
    }
}

/* Draws the shape and its reflections. Reflecting both end points reflects the whole shape since
 * every kind is symmetric about its bounding box. Returns -1 if a pixel could not be recorded. */
static int draw(History *history, const Cel *cel, ShapeStyle style, ToolPoint a, ToolPoint b)
{
    HistoryPlot plot = {history, cel, style.color, false};
    int width = cel_width(cel);
    int height = cel_height(cel);
    for (int i = 0; i < symmetry_count(style.symmetry); i++) {
        draw_one(&plot, style, symmetry_copy(style.symmetry, width, height, a, i),
                 symmetry_copy(style.symmetry, width, height, b, i));
    }
    return plot.failed ? -1 : 0;
}

int shape_stroke(History *history, Cel *cel, ShapeStyle style, ToolPoint from, ToolPoint to)
{
    if (!history || !cel || !valid_kind(style.kind)) {
        return -1;
    }
    if (history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    if (draw(history, cel, style, from, to) != 0) {
        (void)history_cancel_stroke(history);
        return -1;
    }
    return history_end_stroke(history);
}

struct ShapeStroke {
    History *history;
    Cel *cel;
    ShapeStyle style;
    ToolPoint anchor;
};

ShapeStroke *shape_stroke_begin(History *history, Cel *cel, ShapeStyle style, ToolPoint at)
{
    if (!history || !cel || !valid_kind(style.kind)) {
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
    *stroke = (ShapeStroke){history, cel, style, at};
    if (draw(history, cel, style, at, at) != 0) {
        shape_stroke_cancel(stroke);
        return NULL;
    }
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
    return draw(stroke->history, stroke->cel, stroke->style, stroke->anchor, (ToolPoint){x, y});
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
