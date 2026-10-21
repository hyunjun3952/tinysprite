#include "tinysprite/bucket.h"

#include "tinysprite/raster.h"

typedef struct FillTarget {
    History *history;
    Rgba color;
} FillTarget;

static void plot_to_history(int x, int y, void *user)
{
    const FillTarget *target = user;
    (void)history_set_pixel(target->history, x, y, target->color);
}

int bucket_fill(History *history, Cel *cel, int x, int y, Rgba color)
{
    if (!history || !cel || !cel_in_bounds(cel, x, y)) {
        return -1;
    }
    if (history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    FillTarget target = {history, color};
    if (raster_flood_fill(cel, x, y, plot_to_history, &target) != 0) {
        (void)history_cancel_stroke(history);
        return -1;
    }
    return history_end_stroke(history);
}
