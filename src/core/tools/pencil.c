#include "tinysprite/pencil.h"

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

int pencil_stroke(History *history, Cel *cel, const ToolPoint *points, int count, Rgba color)
{
    if (!history || !cel || !points || count < 1) {
        return -1;
    }
    if (history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    PlotTarget target = {history, color};
    if (count == 1) {
        raster_line(points[0].x, points[0].y, points[0].x, points[0].y, plot_to_history, &target);
    }
    for (int i = 1; i < count; i++) {
        raster_line(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, plot_to_history,
                    &target);
    }
    return history_end_stroke(history);
}

int eraser_stroke(History *history, Cel *cel, const ToolPoint *points, int count)
{
    return pencil_stroke(history, cel, points, count, (Rgba){0, 0, 0, 0});
}
