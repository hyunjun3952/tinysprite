#include "tinysprite/mask.hpp"

#include <algorithm>
#include <climits>

Mask Mask::build(Rect area, const std::function<bool(int, int)> &in)
{
    int x0 = INT_MAX;
    int y0 = INT_MAX;
    int x1 = INT_MIN;
    int y1 = INT_MIN;
    for (int y = area.y; y < area.y + area.h; y++) {
        for (int x = area.x; x < area.x + area.w; x++) {
            if (in(x, y)) {
                x0 = std::min(x0, x);
                y0 = std::min(y0, y);
                x1 = std::max(x1, x);
                y1 = std::max(y1, y);
            }
        }
    }
    Mask mask;
    if (x1 < x0) {
        return mask;
    }
    mask.bounds_ = Rect{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
    mask.bits_.assign(static_cast<size_t>(mask.bounds_.w) * static_cast<size_t>(mask.bounds_.h), 0);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            if (in(x, y)) {
                mask.bits_[static_cast<size_t>(y - y0) * static_cast<size_t>(mask.bounds_.w) +
                           static_cast<size_t>(x - x0)] = 1;
                mask.count_++;
            }
        }
    }
    return mask;
}

Mask Mask::full(Rect area)
{
    if (rect_is_empty(area)) {
        return {};
    }
    return build(area, [](int, int) { return true; });
}

Mask Mask::fromPlot(int width, int height, const std::function<void(const RasterPlotFn &)> &run)
{
    if (width < 1 || height < 1) {
        return {};
    }
    std::vector<unsigned char> seen(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
    run([&](int x, int y) {
        if (x >= 0 && y >= 0 && x < width && y < height) {
            seen[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = 1;
        }
    });
    return build(Rect{0, 0, width, height}, [&](int x, int y) {
        return seen[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] !=
               0;
    });
}

bool Mask::contains(int x, int y) const
{
    return rect_contains(bounds_, x, y) &&
           bits_[static_cast<size_t>(y - bounds_.y) * static_cast<size_t>(bounds_.w) +
                 static_cast<size_t>(x - bounds_.x)] != 0;
}

Mask Mask::translated(int dx, int dy) const
{
    if (empty()) {
        return {};
    }
    Mask moved = *this;
    moved.bounds_.x += dx;
    moved.bounds_.y += dy;
    return moved;
}

Mask Mask::flipped(bool horizontal) const
{
    const Rect b = bounds_;
    return build(b, [&](int x, int y) {
        const int fx = horizontal ? b.x + (b.x + b.w - 1 - x) : x;
        const int fy = horizontal ? y : b.y + (b.y + b.h - 1 - y);
        return contains(fx, fy);
    });
}

Mask Mask::rotated(bool clockwise) const
{
    if (empty()) {
        return {};
    }
    const Rect b = bounds_;
    /* The turned rectangle shares the old one's center, as Editor::rotateSelection does. */
    const Rect turned = {b.x + (b.w - b.h) / 2, b.y + (b.h - b.w) / 2, b.h, b.w};
    return build(turned, [&](int x, int y) {
        const int u = x - turned.x; /* column and row in the turned rectangle */
        const int v = y - turned.y;
        /* clockwise, an old (x, y) goes to (b.h - 1 - y, x): the inverse of that picks the old
         * pixel */
        const int ox = clockwise ? v : b.w - 1 - v;
        const int oy = clockwise ? b.h - 1 - u : u;
        return contains(b.x + ox, b.y + oy);
    });
}

Mask Mask::clipped(Rect area) const
{
    return build(bounds_,
                 [&](int x, int y) { return rect_contains(area, x, y) && contains(x, y); });
}

bool Mask::operator==(const Mask &other) const
{
    const Rect a = bounds_;
    const Rect b = other.bounds_;
    const bool sameBounds = a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
    return count_ == other.count_ && (count_ == 0 || (sameBounds && bits_ == other.bits_));
}

Mask Mask::united(const Mask &other) const
{
    if (empty()) {
        return other;
    }
    if (other.empty()) {
        return *this;
    }
    return build(rect_union(bounds_, other.bounds_),
                 [&](int x, int y) { return contains(x, y) || other.contains(x, y); });
}

Mask Mask::subtracted(const Mask &other) const
{
    return build(bounds_, [&](int x, int y) { return contains(x, y) && !other.contains(x, y); });
}

Mask mask_of_region(const Cel &cel, int x, int y, const FillOptions &options)
{
    return Mask::fromPlot(cel.width(), cel.height(), [&](const RasterPlotFn &plot) {
        raster_flood_fill(cel, x, y, plot, options);
    });
}

Mask mask_of_polygon(int width, int height, const std::vector<std::pair<int, int>> &points)
{
    const Rect clip = {0, 0, width, height};
    return Mask::fromPlot(width, height, [&](const RasterPlotFn &plot) {
        raster_polygon(points, true, plot, &clip);
    });
}

void Mask::forEach(const std::function<void(int, int)> &visit) const
{
    for (int y = bounds_.y; y < bounds_.y + bounds_.h; y++) {
        for (int x = bounds_.x; x < bounds_.x + bounds_.w; x++) {
            if (contains(x, y)) {
                visit(x, y);
            }
        }
    }
}
