#include "tinysprite/bucket.h"

#include "history_plot.h"
#include "tinysprite/raster.h"

int bucket_fill(History *history, Cel *cel, int x, int y, Rgba color, Symmetry symmetry)
{
    if (!history || !cel || !cel_in_bounds(cel, x, y)) {
        return -1;
    }
    if (history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    HistoryPlot plot = {history, cel, color, false};
    ToolPoint seeds[4];
    int count =
        symmetry_points(symmetry, cel_width(cel), cel_height(cel), (ToolPoint){x, y}, seeds);
    for (int i = 0; i < count; i++) {
        if (raster_flood_fill(cel, seeds[i].x, seeds[i].y, history_plot, &plot) != 0 ||
            plot.failed) {
            (void)history_cancel_stroke(history);
            return -1;
        }
    }
    return history_end_stroke(history);
}
