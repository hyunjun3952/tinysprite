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
    return bucket_fill_ex(history, cel, x, y, color, (Symmetry){0});
}

int bucket_fill_ex(History *history, Cel *cel, int x, int y, Rgba color, Symmetry symmetry)
{
    if (!history || !cel || !cel_in_bounds(cel, x, y)) {
        return -1;
    }
    if (history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    FillTarget target = {history, color};
    int xs[2] = {x, cel_width(cel) - 1 - x};
    int ys[2] = {y, cel_height(cel) - 1 - y};
    for (int j = 0; j <= (symmetry.mirror_y != 0); j++) {
        for (int i = 0; i <= (symmetry.mirror_x != 0); i++) {
            /* A seed on a mirror axis is its own reflection: fill it once. */
            if ((i == 1 && xs[1] == xs[0]) || (j == 1 && ys[1] == ys[0])) {
                continue;
            }
            if (raster_flood_fill(cel, xs[i], ys[j], plot_to_history, &target) != 0) {
                (void)history_cancel_stroke(history);
                return -1;
            }
        }
    }
    return history_end_stroke(history);
}
