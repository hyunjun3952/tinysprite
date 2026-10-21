#include "tinysprite/tools.h"

#include <cstdlib>

#include "history_plot.h"
#include "tinysprite/raster.h"

bool pencil_stroke(History &history, Cel &cel, const std::vector<ToolPoint> &points, Rgba color)
{
    if (points.empty() || !history.beginStroke(cel)) {
        return false;
    }
    RasterPlotFn plot = history_plot(history, cel, color);
    if (points.size() == 1) {
        raster_line(points[0].x, points[0].y, points[0].x, points[0].y, plot);
    }
    for (size_t i = 1; i < points.size(); i++) {
        raster_line(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, plot);
    }
    return history.endStroke();
}

int PencilOptions::brushSize() const
{
    if (size < 1) {
        return 1;
    }
    return size > PENCIL_BRUSH_MAX ? PENCIL_BRUSH_MAX : size;
}

int PencilOptions::brushOrigin() const
{
    return (brushSize() - 1) / 2;
}

/* A round brush leaves out the pixels whose center lies too far from the brush center:
 * (2i - size + 1) is twice the offset from it, and size * size - size is the squared distance
 * (times four) that trims the corners off while keeping the edge midpoints, so size 3 becomes a
 * plus and larger sizes get rounded corners. */
bool PencilOptions::brushContains(int i, int j) const
{
    int n = brushSize();
    if (i < 0 || j < 0 || i >= n || j >= n) {
        return false;
    }
    if (!round) {
        return true;
    }
    int dx = 2 * i - n + 1;
    int dy = 2 * j - n + 1;
    return dx * dx + dy * dy <= n * n - n;
}

PencilStroke::PencilStroke(History &history, Cel &cel, Rgba color, PencilOptions options, int x,
                           int y)
    : LiveStroke(history, cel), color_(color), options_(options), lastX_(x), lastY_(y)
{
    plotPixel(x, y);
}

std::unique_ptr<PencilStroke> PencilStroke::begin(History &history, Cel &cel, Rgba color,
                                                  PencilOptions options, int x, int y)
{
    if (history.strokeActive()) {
        return nullptr;
    }
    return std::unique_ptr<PencilStroke>(new PencilStroke(history, cel, color, options, x, y));
}

/* Paints (x, y) and its reflections, and returns what was touched. */
PencilStroke::Placed PencilStroke::placePixel(int x, int y)
{
    Placed placed;
    placed.x = x;
    placed.y = y;
    for (ToolPoint at : symmetry_points(options_.symmetry, cel_.width(), cel_.height(), {x, y})) {
        if (cel_.inBounds(at.x, at.y)) {
            placed.copies.push_back({at, cel_.get(at.x, at.y)});
            history_.setPixel(at.x, at.y, color_);
        }
    }
    return placed;
}

/* The history records the restore as one more delta, so undoing the stroke still ends at the
 * colors from before it. */
void PencilStroke::unplacePixel(const Placed &placed)
{
    for (const Placed::Copy &copy : placed.copies) {
        history_.setPixel(copy.at.x, copy.at.y, copy.before);
    }
}

/* Paints the whole brush at (x, y), mirroring each of its pixels separately so a reflected stamp
 * is the exact mirror image even for an even size. */
void PencilStroke::stampBrush(int size, int x, int y)
{
    int origin = options_.brushOrigin();
    for (int j = 0; j < size; j++) {
        for (int i = 0; i < size; i++) {
            if (options_.brushContains(i, j)) {
                placePixel(x - origin + i, y - origin + j);
            }
        }
    }
}

static bool isAdjacent(int x0, int y0, int x1, int y1)
{
    return std::abs(x0 - x1) + std::abs(y0 - y1) == 1;
}

/* a, b and the new point (x, y) form an L: a-b and b-new are steps along different axes. */
static bool makesCorner(int ax, int ay, int bx, int by, int x, int y)
{
    return isAdjacent(ax, ay, bx, by) && isAdjacent(bx, by, x, y) && std::abs(ax - x) == 1 &&
           std::abs(ay - y) == 1;
}

void PencilStroke::plotPixelPerfect(int x, int y)
{
    /* Joining segments plots the shared point twice. */
    if (!window_.empty() && window_.back().x == x && window_.back().y == y) {
        return;
    }
    if (window_.size() == 2) {
        const Placed &a = window_[0];
        const Placed &b = window_[1];
        if (makesCorner(a.x, a.y, b.x, b.y, x, y)) {
            /* Drop the middle pixel so the outer two touch diagonally. */
            unplacePixel(b);
            window_[1] = placePixel(x, y);
            return;
        }
        window_.erase(window_.begin());
    }
    window_.push_back(placePixel(x, y));
}

void PencilStroke::plotPixel(int x, int y)
{
    int size = options_.brushSize();
    if (size > 1) {
        stampBrush(size, x, y);
    } else if (options_.pixel_perfect) {
        plotPixelPerfect(x, y);
    } else {
        placePixel(x, y);
    }
}

void PencilStroke::extend(int x, int y)
{
    raster_line(lastX_, lastY_, x, y, [this](int px, int py) { plotPixel(px, py); });
    lastX_ = x;
    lastY_ = y;
}
