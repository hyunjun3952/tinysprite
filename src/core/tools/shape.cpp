#include "tinysprite/tools.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <vector>

#include "history_plot.h"
#include "tinysprite/raster.h"

namespace
{

using Points = std::vector<std::pair<int, int>>;

/* A plus sign: the arms have the parity of the box, so they can sit exactly in the middle. */
Points crossPoints(int x0, int y0, int x1, int y1)
{
    int w = x1 - x0;
    int h = y1 - y0;
    int armW = std::min(w, std::max(1, w / 3));
    int armH = std::min(h, std::max(1, h / 3));
    armW += (w - armW) % 2;
    armH += (h - armH) % 2;
    int left = x0 + (w - armW) / 2;
    int right = left + armW;
    int top = y0 + (h - armH) / 2;
    int bottom = top + armH;
    return {{left, y0},  {right, y0}, {right, top},   {x1, top},    {x1, bottom}, {right, bottom},
            {right, y1}, {left, y1},  {left, bottom}, {x0, bottom}, {x0, top},    {left, top}};
}

Points starPoints(int x0, int y0, int x1, int y1)
{
    constexpr double PI = 3.14159265358979323846;
    constexpr double INNER = 0.4; /* inner radius / outer radius */
    int w = x1 - x0;
    int h = y1 - y0;
    /* A unit star spans x in +-sin(72 deg) and y from -1 to cos(36 deg); stretch that to the
     * box so its points touch every side. */
    const double halfWidth = std::sin(2.0 * PI / 5.0);
    const double depth = 1.0 + std::cos(PI / 5.0);
    double cx = x0 + w / 2.0;
    Points points;
    for (int i = 0; i < 10; i++) {
        double angle = -PI / 2.0 + i * PI / 5.0;
        double r = i % 2 == 0 ? 1.0 : INNER;
        double px = cx + r * std::cos(angle) / halfWidth * (w / 2.0);
        double py = y0 + (r * std::sin(angle) + 1.0) / depth * h;
        points.emplace_back(static_cast<int>(std::lround(px)), static_cast<int>(std::lround(py)));
    }
    return points;
}

/* The corners of `kind` inside the box spanned by a and b (both inclusive). */
Points polygonPoints(ShapeKind kind, int x0, int y0, int x1, int y1)
{
    int w = x1 - x0;
    int h = y1 - y0;
    int mx = x0 + w / 2;
    int my = y0 + h / 2;
    switch (kind) {
    case ShapeKind::Triangle:
        return {{mx, y0}, {x1, y1}, {x0, y1}};
    case ShapeKind::Diamond:
        return {{mx, y0}, {x1, my}, {mx, y1}, {x0, my}};
    case ShapeKind::Hexagon:
        return {{x0 + w / 4, y0}, {x1 - w / 4, y0}, {x1, my},
                {x1 - w / 4, y1}, {x0 + w / 4, y1}, {x0, my}};
    case ShapeKind::Cross:
        return crossPoints(x0, y0, x1, y1);
    case ShapeKind::Star:
        return starPoints(x0, y0, x1, y1);
    default:
        return {};
    }
}

void drawOne(const RasterPlotFn &plot, const ShapeStyle &style, ToolPoint a, ToolPoint b)
{
    switch (style.kind) {
    case ShapeKind::Line:
        raster_line(a.x, a.y, b.x, b.y, plot);
        break;
    case ShapeKind::Rect:
        raster_rect(a.x, a.y, b.x, b.y, style.filled, plot);
        break;
    case ShapeKind::Ellipse:
        raster_ellipse(a.x, a.y, b.x, b.y, style.filled, plot);
        break;
    case ShapeKind::Triangle:
    case ShapeKind::Diamond:
    case ShapeKind::Hexagon:
    case ShapeKind::Star:
    case ShapeKind::Cross:
        raster_polygon(polygonPoints(style.kind, std::min(a.x, b.x), std::min(a.y, b.y),
                                     std::max(a.x, b.x), std::max(a.y, b.y)),
                       style.filled, plot);
        break;
    }
}

/* Draws the shape and its reflections. Reflecting both end points reflects the whole shape since
 * every kind is symmetric about its bounding box. */
void draw(History &history, const Cel &cel, const ShapeStyle &style, ToolPoint a, ToolPoint b)
{
    RasterPlotFn plot = history_plot(history, cel, style.color);
    for (int i = 0; i < symmetry_count(style.symmetry); i++) {
        drawOne(plot, style, symmetry_copy(style.symmetry, cel.width(), cel.height(), a, i),
                symmetry_copy(style.symmetry, cel.width(), cel.height(), b, i));
    }
}

} // namespace

ToolPoint shape_constrain(ShapeKind kind, ToolPoint anchor, ToolPoint to)
{
    int dx = to.x - anchor.x;
    int dy = to.y - anchor.y;
    int ax = std::abs(dx);
    int ay = std::abs(dy);
    int sx = dx < 0 ? -1 : 1;
    int sy = dy < 0 ? -1 : 1;
    if (kind == ShapeKind::Line) {
        if (ax > 2 * ay) {
            return ToolPoint{to.x, anchor.y};
        }
        if (ay > 2 * ax) {
            return ToolPoint{anchor.x, to.y};
        }
    }
    int side = std::max(ax, ay);
    return ToolPoint{anchor.x + sx * side, anchor.y + sy * side};
}

bool shape_stroke(History &history, Cel &cel, ShapeStyle style, ToolPoint from, ToolPoint to)
{
    if (!history.beginStroke(cel)) {
        return false;
    }
    draw(history, cel, style, from, to);
    return history.endStroke();
}

ShapeStroke::ShapeStroke(History &history, Cel &cel, ShapeStyle style, ToolPoint anchor)
    : LiveStroke(history, cel), style_(style), anchor_(anchor)
{
    draw(history_, cel_, style_, anchor_, anchor_);
}

std::unique_ptr<ShapeStroke> ShapeStroke::begin(History &history, Cel &cel, ShapeStyle style,
                                                ToolPoint at)
{
    if (history.strokeActive()) {
        return nullptr;
    }
    return std::unique_ptr<ShapeStroke>(new ShapeStroke(history, cel, style, at));
}

void ShapeStroke::update(int x, int y)
{
    restart();
    draw(history_, cel_, style_, anchor_, ToolPoint{x, y});
}
