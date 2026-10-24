#include "tinysprite/raster.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <vector>

void raster_line(int x0, int y0, int x1, int y1, const RasterPlotFn &plot)
{
    long long dx = std::llabs(static_cast<long long>(x1) - x0);
    long long dy = -std::llabs(static_cast<long long>(y1) - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    long long err = dx + dy;

    for (;;) {
        plot(x0, y0);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        long long e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* The cels x0..x1 of row y; with a clip, only those inside it. */
static void hspan(int x0, int x1, int y, const RasterPlotFn &plot, const Rect *clip = nullptr)
{
    if (clip) {
        if (y < clip->y || y - clip->y >= clip->h) {
            return;
        }
        x0 = std::max(x0, clip->x);
        x1 = std::min(x1, clip->x + clip->w - 1);
    }
    for (int x = x0; x <= x1; x++) {
        plot(x, y);
    }
}

static void order(int &a, int &b)
{
    if (a > b) {
        std::swap(a, b);
    }
}

void raster_rect(int x0, int y0, int x1, int y1, bool filled, const RasterPlotFn &plot,
                 const Rect *clip)
{
    order(x0, x1);
    order(y0, y1);

    if (filled) {
        int top = clip ? std::max(y0, clip->y) : y0;
        int bottom = clip ? std::min(y1, clip->y + clip->h - 1) : y1;
        for (int y = top; y <= bottom; y++) {
            hspan(x0, x1, y, plot, clip);
        }
        return;
    }

    hspan(x0, x1, y0, plot);
    if (y1 > y0) {
        hspan(x0, x1, y1, plot);
    }
    for (int y = y0 + 1; y < y1; y++) {
        plot(x0, y);
        if (x1 > x0) {
            plot(x1, y);
        }
    }
}

/* The pixels of one step of the ellipse walk: the four mirrored points of the outline, or for a
 * filled ellipse the two spans of a new row (the middle row of an odd height is one span). */
static void ellipse_row(long long lx, long long rx, long long up, long long dn, bool filled,
                        bool new_row, const RasterPlotFn &plot, const Rect *clip)
{
    if (filled) {
        if (new_row) {
            hspan(static_cast<int>(lx), static_cast<int>(rx), static_cast<int>(dn), plot, clip);
            if (up != dn) {
                hspan(static_cast<int>(lx), static_cast<int>(rx), static_cast<int>(up), plot, clip);
            }
        }
        return;
    }
    plot(static_cast<int>(rx), static_cast<int>(dn));
    plot(static_cast<int>(lx), static_cast<int>(dn));
    if (up != dn) {
        plot(static_cast<int>(lx), static_cast<int>(up));
        plot(static_cast<int>(rx), static_cast<int>(up));
    }
}

/* A flat ellipse stops the walk before its top and bottom rows are reached; finishes the
 * remaining rows at the tips. */
static void ellipse_tips(long long lx, long long rx, long long up, long long dn, long long b,
                         bool filled, const RasterPlotFn &plot, const Rect *clip)
{
    while (dn - up <= b) {
        if (filled) {
            hspan(static_cast<int>(lx) - 1, static_cast<int>(rx) + 1, static_cast<int>(dn), plot,
                  clip);
            hspan(static_cast<int>(lx) - 1, static_cast<int>(rx) + 1, static_cast<int>(up), plot,
                  clip);
        } else {
            plot(static_cast<int>(lx) - 1, static_cast<int>(dn));
            plot(static_cast<int>(rx) + 1, static_cast<int>(dn));
            plot(static_cast<int>(lx) - 1, static_cast<int>(up));
            plot(static_cast<int>(rx) + 1, static_cast<int>(up));
        }
        dn++;
        up--;
    }
}

/* Midpoint ellipse over the bounding box, after Alois Zingl's "rasterizing ellipses in a
 * rectangle". Walks the upper-left quadrant outwards and mirrors every step into the other three
 * (lx/rx are the left and right columns, up/dn the upper and lower rows). `err` is the error term
 * of the next diagonal step; dx and dy are its increments for stepping in x or in y. A box with an
 * even height has two middle rows, so up starts one row above dn (b1 is 1 for an even height). */
void raster_ellipse(int x0, int y0, int x1, int y1, bool filled, const RasterPlotFn &plot,
                    const Rect *clip)
{
    order(x0, x1);
    order(y0, y1);

    long long a = static_cast<long long>(x1) - x0;
    long long b = static_cast<long long>(y1) - y0;
    if (a > RASTER_ELLIPSE_MAX_SPAN || b > RASTER_ELLIPSE_MAX_SPAN) {
        return;
    }
    long long b1 = b & 1;
    long long dx = 4 * (1 - a) * b * b;
    long long dy = 4 * (b1 + 1) * a * a;
    long long err = dx + dy + b1 * a * a;

    long long lx = x0;
    long long rx = x1;
    long long dn = y0 + (b + 1) / 2;
    long long up = dn - b1;
    int new_row = 1;
    long long step_a = 8 * a * a;
    long long step_b = 8 * b * b;

    do {
        ellipse_row(lx, rx, up, dn, filled, new_row != 0, plot, clip);
        new_row = 0;

        long long e2 = 2 * err;
        if (e2 <= dy) {
            dn++;
            up--;
            dy += step_a;
            err += dy;
            new_row = 1;
        }
        if (e2 >= dx || 2 * err > dy) {
            lx++;
            rx--;
            dx += step_b;
            err += dx;
        }
    } while (lx <= rx);

    ellipse_tips(lx, rx, up, dn, b, filled, plot, clip);
}

/* The x where each polygon edge crosses the scanline y, sorted. Edges are half-open in y, so a
 * vertex shared by two edges counts once. */
static void scanline_crossings(const std::vector<std::pair<int, int>> &points, int y,
                               std::vector<double> &crossings)
{
    const size_t n = points.size();
    crossings.clear();
    for (size_t i = 0; i < n; i++) {
        const auto &a = points[i];
        const auto &b = points[(i + 1) % n];
        if ((a.second <= y && b.second > y) || (b.second <= y && a.second > y)) {
            double t = (y - a.second) / static_cast<double>(b.second - a.second);
            crossings.push_back(a.first + t * (b.first - a.first));
        }
    }
    std::sort(crossings.begin(), crossings.end());
}

/* Scanline fill: for every row, the crossings with the edges, paired up into spans (even-odd). */
static void fill_polygon(const std::vector<std::pair<int, int>> &points, const RasterPlotFn &plot,
                         const Rect *clip)
{
    int top = points[0].second;
    int bottom = points[0].second;
    for (const auto &p : points) {
        top = std::min(top, p.second);
        bottom = std::max(bottom, p.second);
    }
    if (clip) {
        top = std::max(top, clip->y);
        bottom = std::min(bottom, clip->y + clip->h - 1);
    }
    std::vector<double> crossings;
    for (int y = top; y <= bottom; y++) {
        scanline_crossings(points, y, crossings);
        for (size_t i = 0; i + 1 < crossings.size(); i += 2) {
            /* Pixels sit on integer coordinates; those inside the span, ends included, are
             * lit, which keeps a mirrored polygon an exact mirror image. */
            int from = static_cast<int>(std::ceil(crossings[i]));
            int to = static_cast<int>(std::floor(crossings[i + 1]));
            hspan(from, to, y, plot, clip);
        }
    }
}

void raster_polygon(const std::vector<std::pair<int, int>> &points, bool filled,
                    const RasterPlotFn &plot, const Rect *clip)
{
    if (points.empty()) {
        return;
    }
    if (points.size() == 1) {
        plot(points[0].first, points[0].second);
        return;
    }
    if (filled) {
        fill_polygon(points, plot, clip);
    }
    const size_t n = points.size();
    for (size_t i = 0; i < n; i++) {
        auto a = points[i];
        auto b = points[(i + 1) % n];
        /* Always top to bottom: Bresenham is only mirror-symmetric for a fixed vertical direction,
         * so a mirrored polygon keeps mirrored edges. */
        if (a.second > b.second) {
            std::swap(a, b);
        }
        raster_line(a.first, a.second, b.first, b.second, plot);
    }
}

namespace
{

/* Whether `pixel` is within `tolerance` of `target` in every channel. */
bool within(Rgba pixel, Rgba target, int tolerance)
{
    return std::abs(pixel.r - target.r) <= tolerance && std::abs(pixel.g - target.g) <= tolerance &&
           std::abs(pixel.b - target.b) <= tolerance && std::abs(pixel.a - target.a) <= tolerance;
}

/* The state of one flood fill: which pixels were taken and which still match the clicked color. */
class FloodRegion
{
  public:
    FloodRegion(const Cel &cel, Rgba target, int tolerance)
        : cel_(cel), target_(target), tolerance_(tolerance),
          seen_(static_cast<size_t>(cel.width()) * static_cast<size_t>(cel.height()), 0)
    {
    }

    bool fillable(int x, int y) const
    {
        return seen_[cel_.indexOf(x, y)] == 0 && within(cel_.get(x, y), target_, tolerance_);
    }
    bool taken(int x, int y) const
    {
        return seen_[cel_.indexOf(x, y)] != 0;
    }

    /* Takes the whole run of fillable pixels around (x, y) in its row; returns it as [left, right].
     */
    std::pair<int, int> takeRun(int x, int y)
    {
        int left = x;
        int right = x;
        while (left > 0 && fillable(left - 1, y)) {
            left--;
        }
        while (right < cel_.width() - 1 && fillable(right + 1, y)) {
            right++;
        }
        for (int i = left; i <= right; i++) {
            seen_[cel_.indexOf(i, y)] = 1;
        }
        return {left, right};
    }

    /* Queues one seed per run of fillable pixels in row y, between x0 and x1. */
    void pushRuns(std::vector<std::pair<int, int>> &stack, int x0, int x1, int y) const
    {
        if (y < 0 || y >= cel_.height()) {
            return;
        }
        bool inRun = false;
        for (int x = x0; x <= x1; x++) {
            bool open = fillable(x, y);
            if (open && !inRun) {
                stack.emplace_back(x, y);
            }
            inRun = open;
        }
    }

  private:
    const Cel &cel_;
    Rgba target_;
    int tolerance_;
    std::vector<unsigned char> seen_;
};

/* Reports every pixel of the cel that matches the clicked color, connected or not. */
void plotMatching(const Cel &cel, int x, int y, int tolerance, const RasterPlotFn &plot)
{
    const Rgba target = cel.get(x, y);
    std::vector<std::pair<int, int>> matching;
    for (int r = 0; r < cel.height(); r++) {
        for (int c = 0; c < cel.width(); c++) {
            if (within(cel.get(c, r), target, tolerance)) {
                matching.emplace_back(c, r);
            }
        }
    }
    /* Known before the first plot, so the callback can edit the cel it came from. */
    for (const auto &[c, r] : matching) {
        plot(c, r);
    }
}

} // namespace

bool raster_flood_fill(const Cel &cel, int x, int y, const RasterPlotFn &plot,
                       const FillOptions &options)
{
    if (!cel.inBounds(x, y)) {
        return false;
    }
    const int tolerance = std::clamp(options.tolerance, 0, 255);
    if (!options.contiguous) {
        plotMatching(cel, x, y, tolerance, plot);
        return true;
    }
    FloodRegion region(cel, cel.get(x, y), tolerance);
    std::vector<std::pair<int, int>> stack = {{x, y}};
    while (!stack.empty()) {
        auto [sx, sy] = stack.back();
        stack.pop_back();
        if (region.taken(sx, sy)) {
            continue;
        }
        auto [left, right] = region.takeRun(sx, sy);
        region.pushRuns(stack, left, right, sy - 1);
        region.pushRuns(stack, left, right, sy + 1);
    }

    /* Report only once the region is known, so the callback can edit the cel it came from. */
    for (int r = 0; r < cel.height(); r++) {
        for (int c = 0; c < cel.width(); c++) {
            if (region.taken(c, r)) {
                plot(c, r);
            }
        }
    }
    return true;
}
