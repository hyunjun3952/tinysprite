#ifndef TINYSPRITE_HISTORY_PLOT_H
#define TINYSPRITE_HISTORY_PLOT_H

#include "tinysprite/history.h"

/* Where a rasterizer's pixels go: into the history's open stroke on `cel`, in one color.
 * Pixels outside the cel are skipped, which is how every tool clips. The only other way a plot
 * can fail is allocation, so `failed` records that and the tool then abandons its stroke instead
 * of committing a drawing with holes in it. */
typedef struct HistoryPlot {
    History *history;
    const Cel *cel;
    Rgba color;
    bool failed;
} HistoryPlot;

/* A RasterPlotFn; `user` is a HistoryPlot. */
void history_plot(int x, int y, void *user);

#endif
