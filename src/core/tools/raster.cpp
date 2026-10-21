#include "tinysprite/raster.h"

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

static void hspan(int x0, int x1, int y, const RasterPlotFn &plot)
{
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

void raster_rect(int x0, int y0, int x1, int y1, bool filled, const RasterPlotFn &plot)
{
    order(x0, x1);
    order(y0, y1);

    if (filled) {
        for (int y = y0; y <= y1; y++) {
            hspan(x0, x1, y, plot);
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

/* Midpoint ellipse over the bounding box, after Alois Zingl's "rasterizing ellipses in a
 * rectangle". Walks the upper-left quadrant outwards and mirrors every step into the other three
 * (lx/rx are the left and right columns, up/dn the upper and lower rows). `err` is the error term
 * of the next diagonal step; dx and dy are its increments for stepping in x or in y. A box with an
 * even height has two middle rows, so up starts one row above dn (b1 is 1 for an even height). */
void raster_ellipse(int x0, int y0, int x1, int y1, bool filled, const RasterPlotFn &plot)
{
    order(x0, x1);
    order(y0, y1);

    long long a = static_cast<long long>(x1) - x0;
    long long b = static_cast<long long>(y1) - y0;
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
        if (filled) {
            if (new_row) {
                hspan(static_cast<int>(lx), static_cast<int>(rx), static_cast<int>(dn), plot);
                if (up != dn) {
                    hspan(static_cast<int>(lx), static_cast<int>(rx), static_cast<int>(up), plot);
                }
            }
        } else {
            plot(static_cast<int>(rx), static_cast<int>(dn));
            plot(static_cast<int>(lx), static_cast<int>(dn));
            if (up != dn) {
                plot(static_cast<int>(lx), static_cast<int>(up));
                plot(static_cast<int>(rx), static_cast<int>(up));
            }
        }
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

    /* A flat ellipse stops the loop above before its top and bottom rows are reached; finish the
     * remaining rows at the tips. */
    while (dn - up <= b) {
        if (filled) {
            hspan(static_cast<int>(lx) - 1, static_cast<int>(rx) + 1, static_cast<int>(dn), plot);
            hspan(static_cast<int>(lx) - 1, static_cast<int>(rx) + 1, static_cast<int>(up), plot);
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

/* Queues one seed per run of fillable pixels in row y, between x0 and x1. */
static void push_runs(std::vector<std::pair<int, int>> &stack, const Cel &cel,
                      const std::vector<unsigned char> &seen, Rgba target, int x0, int x1, int y)
{
    if (y < 0 || y >= cel.height()) {
        return;
    }
    size_t w = static_cast<size_t>(cel.width());
    bool inRun = false;
    for (int x = x0; x <= x1; x++) {
        bool fillable =
            !seen[static_cast<size_t>(y) * w + static_cast<size_t>(x)] && cel.get(x, y) == target;
        if (fillable && !inRun) {
            stack.emplace_back(x, y);
        }
        inRun = fillable;
    }
}

bool raster_flood_fill(const Cel &cel, int x, int y, const RasterPlotFn &plot)
{
    if (!cel.inBounds(x, y)) {
        return false;
    }
    size_t w = static_cast<size_t>(cel.width());
    size_t h = static_cast<size_t>(cel.height());
    Rgba target = cel.get(x, y);
    std::vector<unsigned char> seen(w * h, 0);
    std::vector<std::pair<int, int>> stack = {{x, y}};

    while (!stack.empty()) {
        auto [sx, sy] = stack.back();
        stack.pop_back();
        size_t row = static_cast<size_t>(sy) * w;
        if (seen[row + static_cast<size_t>(sx)]) {
            continue;
        }
        int left = sx;
        int right = sx;
        while (left > 0 && !seen[row + static_cast<size_t>(left) - 1] &&
               cel.get(left - 1, sy) == target) {
            left--;
        }
        while (right < cel.width() - 1 && !seen[row + static_cast<size_t>(right) + 1] &&
               cel.get(right + 1, sy) == target) {
            right++;
        }
        for (int i = left; i <= right; i++) {
            seen[row + static_cast<size_t>(i)] = 1;
        }
        push_runs(stack, cel, seen, target, left, right, sy - 1);
        push_runs(stack, cel, seen, target, left, right, sy + 1);
    }

    /* Report only once the region is known, so the callback can edit the cel it came from. */
    for (size_t r = 0; r < h; r++) {
        for (size_t c = 0; c < w; c++) {
            if (seen[r * w + c]) {
                plot(static_cast<int>(c), static_cast<int>(r));
            }
        }
    }
    return true;
}
