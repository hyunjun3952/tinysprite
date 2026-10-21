#include "tinysprite/rect.h"

#include "tinysprite/util.h"

Rect rect_clip(Rect r, int width, int height)
{
    /* long long: x + w can pass INT_MAX for a selection dragged far outside the cel. */
    long long x0 = max_int(r.x, 0);
    long long y0 = max_int(r.y, 0);
    long long x1 = (long long)r.x + r.w;
    long long y1 = (long long)r.y + r.h;
    if (x1 > width) {
        x1 = width;
    }
    if (y1 > height) {
        y1 = height;
    }
    if (r.w < 1 || r.h < 1 || x0 >= x1 || y0 >= y1) {
        return (Rect){0, 0, 0, 0};
    }
    return (Rect){(int)x0, (int)y0, (int)(x1 - x0), (int)(y1 - y0)};
}

Rect rect_from_corners(int x0, int y0, int x1, int y1)
{
    int left = min_int(x0, x1);
    int top = min_int(y0, y1);
    return (Rect){left, top, max_int(x0, x1) - left + 1, max_int(y0, y1) - top + 1};
}

Rect rect_union(Rect a, Rect b)
{
    int left = min_int(a.x, b.x);
    int top = min_int(a.y, b.y);
    return (Rect){left, top, max_int(a.x + a.w, b.x + b.w) - left,
                  max_int(a.y + a.h, b.y + b.h) - top};
}

Rect rect_grow(Rect r, int amount)
{
    return (Rect){r.x - amount, r.y - amount, r.w + 2 * amount, r.h + 2 * amount};
}
