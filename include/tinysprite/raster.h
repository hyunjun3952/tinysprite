#ifndef TINYSPRITE_RASTER_H
#define TINYSPRITE_RASTER_H

#include "tinysprite/cel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Rasterizers never touch a cel: they report pixels through a callback so that tools can route
 * every edit through the history module. Coordinates may lie outside any cel; callers clip.
 * Line, rectangle and flood fill report each pixel once; ellipses may report a pixel more than
 * once, so consumers must tolerate repeats. */
typedef void (*RasterPlotFn)(int x, int y, void *user);

/* Bresenham line, both endpoints included. */
void raster_line(int x0, int y0, int x1, int y1, RasterPlotFn plot, void *user);

/* Axis-aligned rectangle spanning the two corners (any order, both inclusive). */
void raster_rect(int x0, int y0, int x1, int y1, bool filled, RasterPlotFn plot, void *user);

/* Ellipse inscribed in the bounding box spanned by the two corners (any order, both inclusive). */
void raster_ellipse(int x0, int y0, int x1, int y1, bool filled, RasterPlotFn plot, void *user);

/* Reports the 4-connected region of cel pixels sharing the exact color of (x, y), in row-major
 * order. The cel is only read, so the callback may edit it. Returns 0 on success, -1 if (x, y)
 * is out of bounds or on allocation failure (nothing is reported then). */
int raster_flood_fill(const Cel *cel, int x, int y, RasterPlotFn plot, void *user);

#ifdef __cplusplus
}
#endif

#endif
