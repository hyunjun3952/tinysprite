#include "tinysprite/tools.hpp"

#include "history_plot.hpp"
#include "tinysprite/raster.hpp"

bool bucket_fill(History &history, Cel &cel, int x, int y, Rgba color, Symmetry symmetry)
{
    if (!cel.inBounds(x, y) || !history.beginStroke(cel)) {
        return false;
    }
    RasterPlotFn plot = history_plot(history, cel, color);
    for (ToolPoint seed : symmetry_points(symmetry, cel.width(), cel.height(), {x, y})) {
        raster_flood_fill(cel, seed.x, seed.y, plot);
    }
    return history.endStroke();
}
