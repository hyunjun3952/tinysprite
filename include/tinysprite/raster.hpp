#ifndef TINYSPRITE_RASTER_HPP
#define TINYSPRITE_RASTER_HPP

#include <functional>
#include <utility>
#include <vector>

#include "tinysprite/cel.hpp"
#include "tinysprite/rect.hpp"

/* Rasterizers never touch a cel: they report pixels through a callback so that tools can route
 * every edit through the history module. Coordinates may lie outside any cel; callers clip.
 * Line, rectangle and flood fill report each pixel once; ellipses and polygons may report a pixel
 * more than once, so consumers must tolerate repeats.
 * The filled variants take an optional `clip`: they then skip the rows and columns of the fill
 * outside it, so a shape dragged far past the cel costs only what the cel shows. Outlines are
 * never clipped. */
using RasterPlotFn = std::function<void(int x, int y)>;

/* Bresenham line, both endpoints included. */
void raster_line(int x0, int y0, int x1, int y1, const RasterPlotFn &plot);

/* Axis-aligned rectangle spanning the two corners (any order, both inclusive). */
void raster_rect(int x0, int y0, int x1, int y1, bool filled, const RasterPlotFn &plot,
                 const Rect *clip = nullptr);

/* The widest or tallest box raster_ellipse draws: the walk squares the span, which would overflow
 * 64 bits well past this (a sprite is at most CEL_MAX_DIMENSION wide). */
inline constexpr long long RASTER_ELLIPSE_MAX_SPAN = 1 << 20;

/* Ellipse inscribed in the bounding box spanned by the two corners (any order, both inclusive).
 * A box wider or taller than RASTER_ELLIPSE_MAX_SPAN draws nothing. */
void raster_ellipse(int x0, int y0, int x1, int y1, bool filled, const RasterPlotFn &plot,
                    const Rect *clip = nullptr);

/* Closed polygon through `points` (in order, the last joined back to the first). The outline is
 * always drawn; `filled` also fills the inside (even-odd rule, so concave shapes such as a star
 * keep their gaps). Fewer than two points draw at most a dot. */
void raster_polygon(const std::vector<std::pair<int, int>> &points, bool filled,
                    const RasterPlotFn &plot, const Rect *clip = nullptr);

/* Reports the 4-connected region of cel pixels sharing the exact color of (x, y), in row-major
 * order. The cel is only read, so the callback may edit it. Returns false (reporting nothing) if
 * (x, y) is out of bounds. */
bool raster_flood_fill(const Cel &cel, int x, int y, const RasterPlotFn &plot);

#endif
