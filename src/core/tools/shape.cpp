#include "tinysprite/tools.h"

#include "history_plot.h"
#include "tinysprite/raster.h"

namespace
{

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
