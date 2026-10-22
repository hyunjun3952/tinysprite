#ifndef TINYSPRITE_RECT_HPP
#define TINYSPRITE_RECT_HPP

/* An axis-aligned block of cels: (x, y) is its top-left cel and it spans w x h cels. */
struct Rect {
    int x, y, w, h;
};

/* The part of `r` inside 0..width x 0..height. An empty result has w == h == 0. Safe for extents
 * whose far edge would overflow an int. */
Rect rect_clip(Rect r, int width, int height);

inline bool rect_is_empty(Rect r)
{
    return r.w < 1 || r.h < 1;
}

/* Half-open: the far edge x + w, y + h is outside. */
inline bool rect_contains(Rect r, int x, int y)
{
    return x >= r.x && y >= r.y && x - r.x < r.w && y - r.y < r.h;
}

/* The rectangle spanning two corner cells, given in any order and both included. */
Rect rect_from_corners(int x0, int y0, int x1, int y1);

/* The smallest rectangle covering both. */
Rect rect_union(Rect a, Rect b);

/* `r` with `amount` more cels on every side. */
Rect rect_grow(Rect r, int amount);

#endif
