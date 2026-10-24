#include "tinysprite/rect_pack.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{

/* A horizontal run of the skyline: from x for w cels, everything below y is taken. */
struct Segment {
    int x;
    int y;
    int w;
};

/* The result of one try at a fixed sheet width. */
struct Attempt {
    bool fits = false;
    int width = 0; /* the sheet that was used, edges not padded */
    int height = 0;
    std::vector<Rect> rects;
};

/* The lowest y a rectangle `w` wide can have with its left edge at skyline segment `first`, or -1
 * when it would stick out of the sheet. */
int dropHeight(const std::vector<Segment> &skyline, size_t first, int w, int sheetWidth)
{
    const int x = skyline[first].x;
    if (x + w > sheetWidth) {
        return -1;
    }
    int y = 0;
    for (size_t i = first; i < skyline.size() && skyline[i].x < x + w; i++) {
        y = std::max(y, skyline[i].y);
    }
    return y;
}

/* Replaces `skyline` by `runs` (sorted by x), joining neighbours of equal height. */
void joinRuns(std::vector<Segment> &skyline, const std::vector<Segment> &runs)
{
    skyline.clear();
    for (const Segment &s : runs) {
        if (!skyline.empty() && skyline.back().y == s.y) {
            skyline.back().w += s.w;
        } else {
            skyline.push_back(s);
        }
    }
}

/* Raises the skyline over [x, x + w) to `top`, cutting or removing the runs it covers, and joins
 * neighbours of equal height. */
void raise(std::vector<Segment> &skyline, int x, int w, int top)
{
    std::vector<Segment> next;
    for (const Segment &s : skyline) {
        const int end = s.x + s.w;
        if (end <= x || s.x >= x + w) {
            next.push_back(s);
            continue;
        }
        if (s.x < x) {
            next.push_back({s.x, s.y, x - s.x});
        }
        if (end > x + w) {
            next.push_back({x + w, s.y, end - (x + w)});
        }
    }
    next.push_back({x, top, w});
    std::sort(next.begin(), next.end(),
              [](const Segment &a, const Segment &b) { return a.x < b.x; });
    joinRuns(skyline, next);
}

/* Packs the rectangles (each already grown by the padding) in `order` on a sheet `sheetWidth`
 * wide. */
Attempt tryWidth(const std::vector<Rect> &grown, const std::vector<size_t> &order, int sheetWidth,
                 int padding)
{
    Attempt attempt;
    attempt.rects.assign(grown.size(), Rect{0, 0, 0, 0});
    std::vector<Segment> skyline = {{0, 0, sheetWidth}};
    for (size_t index : order) {
        const Rect &r = grown[index];
        int bestY = -1;
        int bestX = 0;
        for (size_t i = 0; i < skyline.size(); i++) {
            const int y = dropHeight(skyline, i, r.w, sheetWidth);
            if (y >= 0 && (bestY < 0 || y < bestY)) {
                bestY = y;
                bestX = skyline[i].x;
            }
        }
        if (bestY < 0) {
            return attempt;
        }
        raise(skyline, bestX, r.w, bestY + r.h);
        attempt.rects[index] = Rect{bestX, bestY, r.w - padding, r.h - padding};
        attempt.width = std::max(attempt.width, bestX + r.w - padding);
        attempt.height = std::max(attempt.height, bestY + r.h - padding);
    }
    attempt.fits = true;
    return attempt;
}

/* Whether `a` is the better sheet: less area, then closer to a square. */
bool better(const Attempt &a, const Attempt &b)
{
    const long long areaA = static_cast<long long>(a.width) * a.height;
    const long long areaB = static_cast<long long>(b.width) * b.height;
    if (areaA != areaB) {
        return areaA < areaB;
    }
    return std::max(a.width, a.height) < std::max(b.width, b.height);
}

bool validInput(const std::vector<Rect> &sizes, int padding, int maxSize)
{
    /* a padding larger than the sheet could be would overflow the sums below, and cannot fit anyway
     */
    if (sizes.empty() || padding < 0 || maxSize < 1 || padding > maxSize) {
        return false;
    }
    return std::all_of(sizes.begin(), sizes.end(), [maxSize](const Rect &r) {
        return r.w >= 1 && r.h >= 1 && r.w <= maxSize && r.h <= maxSize;
    });
}

} // namespace

std::optional<PackedRects> pack_rects(const std::vector<Rect> &sizes, int padding, int maxSize)
{
    if (!validInput(sizes, padding, maxSize)) {
        return std::nullopt;
    }
    std::vector<Rect> grown;
    long long area = 0;
    int widest = 0;
    long long totalWidth = 0;
    for (const Rect &r : sizes) {
        grown.push_back(Rect{0, 0, r.w + padding, r.h + padding});
        area += static_cast<long long>(r.w + padding) * (r.h + padding);
        widest = std::max(widest, r.w + padding);
        totalWidth += r.w + padding;
    }
    /* tall ones first; ties keep the input order so the result is the same every time */
    std::vector<size_t> order(grown.size());
    std::iota(order.begin(), order.end(), size_t{0});
    std::stable_sort(order.begin(), order.end(), [&grown](size_t a, size_t b) {
        return grown[a].h != grown[b].h ? grown[a].h > grown[b].h : grown[a].w > grown[b].w;
    });

    const int lower =
        std::max(widest, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(area)))));
    const auto limit = std::min<long long>(
        {totalWidth, static_cast<long long>(maxSize) + padding, static_cast<long long>(lower) * 4});
    const auto upper = static_cast<int>(std::max<long long>(limit, lower));
    const int step = std::max(1, lower / 32);
    Attempt best;
    for (int width = lower; width <= upper; width += step) {
        Attempt attempt = tryWidth(grown, order, width, padding);
        if (attempt.fits && attempt.width <= maxSize && attempt.height <= maxSize &&
            (!best.fits || better(attempt, best))) {
            best = std::move(attempt);
        }
    }
    if (!best.fits) {
        return std::nullopt;
    }
    PackedRects packed;
    packed.width = best.width;
    packed.height = best.height;
    packed.rects = std::move(best.rects);
    return packed;
}
