#include "tinysprite/tools.h"

#include <algorithm>
#include <array>
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

Points triangleCorners(int x0, int y0, int x1, int y1)
{
    return {{x0 + (x1 - x0) / 2, y0}, {x1, y1}, {x0, y1}};
}

Points diamondCorners(int x0, int y0, int x1, int y1)
{
    int mx = x0 + (x1 - x0) / 2;
    int my = y0 + (y1 - y0) / 2;
    return {{mx, y0}, {x1, my}, {mx, y1}, {x0, my}};
}

Points hexagonCorners(int x0, int y0, int x1, int y1)
{
    int w = x1 - x0;
    int my = y0 + (y1 - y0) / 2;
    return {{x0 + w / 4, y0}, {x1 - w / 4, y0}, {x1, my},
            {x1 - w / 4, y1}, {x0 + w / 4, y1}, {x0, my}};
}

/* How each kind of shape is drawn inside the box spanned by a and b (both inclusive). */
void drawLine(const RasterPlotFn &plot, ToolPoint a, ToolPoint b, bool, const Rect &)
{
    raster_line(a.x, a.y, b.x, b.y, plot);
}

void drawRect(const RasterPlotFn &plot, ToolPoint a, ToolPoint b, bool filled, const Rect &clip)
{
    raster_rect(a.x, a.y, b.x, b.y, filled, plot, &clip);
}

void drawEllipse(const RasterPlotFn &plot, ToolPoint a, ToolPoint b, bool filled, const Rect &clip)
{
    raster_ellipse(a.x, a.y, b.x, b.y, filled, plot, &clip);
}

template <Points (*corners)(int, int, int, int)>
void drawPolygon(const RasterPlotFn &plot, ToolPoint a, ToolPoint b, bool filled, const Rect &clip)
{
    raster_polygon(
        corners(std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)),
        filled, plot, &clip);
}

struct ShapeSpec {
    bool fillable;
    void (*draw)(const RasterPlotFn &plot, ToolPoint a, ToolPoint b, bool filled, const Rect &clip);
};

/* In ShapeKind order: a kind added to the enum needs its row here (a test draws every kind). */
const std::array<ShapeSpec, SHAPE_COUNT> SHAPES = {{
    {false, drawLine},
    {true, drawRect},
    {true, drawEllipse},
    {true, drawPolygon<triangleCorners>},
    {true, drawPolygon<diamondCorners>},
    {true, drawPolygon<hexagonCorners>},
    {true, drawPolygon<starPoints>},
    {true, drawPolygon<crossPoints>},
}};

/* A kind outside the table (ShapeKind::Count) draws nothing. */
const ShapeSpec &specOf(ShapeKind kind)
{
    static const ShapeSpec NONE = {
        false, [](const RasterPlotFn &, ToolPoint, ToolPoint, bool, const Rect &) {}};
    auto index = static_cast<size_t>(kind);
    return index < SHAPES.size() ? SHAPES[index] : NONE;
}

void drawOne(const RasterPlotFn &plot, const ShapeStyle &style, ToolPoint a, ToolPoint b,
             const Rect &clip)
{
    specOf(style.kind).draw(plot, a, b, style.filled, clip);
}

/* The cels the brush paints around a plotted outline pixel, relative to it; just the pixel itself
 * when the brush is one pixel or the shape is filled. */
std::vector<ToolPoint> brushOffsets(const ShapeStyle &style)
{
    PencilOptions brush;
    brush.size = style.brush_size;
    brush.round = style.brush_round;
    bool outlineOnly = !(style.filled && specOf(style.kind).fillable);
    if (!outlineOnly || brush.brushSize() == 1) {
        return {ToolPoint{0, 0}};
    }
    std::vector<ToolPoint> offsets;
    int origin = brush.brushOrigin();
    for (int j = 0; j < brush.brushSize(); j++) {
        for (int i = 0; i < brush.brushSize(); i++) {
            if (brush.brushContains(i, j)) {
                offsets.push_back(ToolPoint{i - origin, j - origin});
            }
        }
    }
    return offsets;
}

/* Draws the shape once and reflects every pixel it plots (brush stamp included) across each
 * enabled axis, so a reflection is the exact mirror image whatever the kind: a triangle turns
 * upside down under a vertical mirror, and a box of even width does not shift its apex by a cel.
 * The fill is clipped to the cel, which the reflection maps onto itself. */
void draw(History &history, const Cel &cel, const ShapeStyle &style, ToolPoint a, ToolPoint b)
{
    RasterPlotFn plot = history_plot(history, cel, style.color);
    const std::vector<ToolPoint> offsets = brushOffsets(style);
    const Symmetry &sym = style.symmetry;
    const int copies = symmetry_count(sym);
    RasterPlotFn stamp = [&](int x, int y) {
        for (ToolPoint o : offsets) {
            for (int i = 0; i < copies; i++) {
                ToolPoint at =
                    symmetry_copy(sym, cel.width(), cel.height(), ToolPoint{x + o.x, y + o.y}, i);
                plot(at.x, at.y);
            }
        }
    };
    drawOne(stamp, style, a, b, Rect{0, 0, cel.width(), cel.height()});
}

} // namespace

bool shape_has_fill(ShapeKind kind)
{
    return specOf(kind).fillable;
}

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
