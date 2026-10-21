#ifndef TINYSPRITE_HISTORY_PLOT_H
#define TINYSPRITE_HISTORY_PLOT_H

#include "tinysprite/history.h"
#include "tinysprite/raster.h"

/* Where a rasterizer's pixels go: into the history's open stroke on `cel`, in one color. Pixels
 * outside the cel are skipped, which is how every tool clips. The cel and history must outlive the
 * returned callback. */
inline RasterPlotFn history_plot(History &history, const Cel &cel, Rgba color)
{
    return [&history, &cel, color](int x, int y) {
        if (cel.inBounds(x, y)) {
            history.setPixel(x, y, color);
        }
    };
}

#endif
