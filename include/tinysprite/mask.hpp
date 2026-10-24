#ifndef TINYSPRITE_MASK_HPP
#define TINYSPRITE_MASK_HPP

#include <functional>
#include <vector>

#include "tinysprite/raster.hpp"
#include "tinysprite/rect.hpp"

/*
 * A set of pixels of a cel of any shape: what a selection is when it is not a rectangle (a region
 * picked by color, a lasso). It is kept over its own bounding rectangle, trimmed to the pixels it
 * holds, and is a plain value: every operation returns a new mask.
 */
class Mask
{
  public:
    /* The empty mask: no pixels, bounds of 0 x 0. */
    Mask() = default;

    /* Every pixel of `area` (a rectangular selection as a mask); empty for an empty rectangle. */
    static Mask full(Rect area);

    /*
     * The pixels `run` plots, which may be a rasterizer or a flood fill, within a width x height
     * area (a cel); a pixel outside it is ignored and a pixel plotted twice counts once. The result
     * is trimmed to what was plotted.
     */
    static Mask fromPlot(int width, int height,
                         const std::function<void(const RasterPlotFn &)> &run);

    bool empty() const
    {
        return count_ == 0;
    }
    /* The number of pixels in the mask. */
    int count() const
    {
        return count_;
    }
    /* The smallest rectangle that holds every pixel; 0 x 0 for an empty mask. */
    Rect bounds() const
    {
        return bounds_;
    }
    /* False for a pixel outside the bounds. */
    bool contains(int x, int y) const;

    /* The same shape moved by (dx, dy). */
    Mask translated(int dx, int dy) const;
    /* Mirrored left to right, or top to bottom, inside its own rectangle. */
    Mask flipped(bool horizontal) const;
    /* Turned a quarter turn inside its own rectangle; the turned rectangle shares the old one's
     * center (the rule Editor::rotateSelection uses for a rectangle). */
    Mask rotated(bool clockwise) const;
    /* The pixels that lie inside `area` (what is left of a mask moved partly off a cel). */
    Mask clipped(Rect area) const;
    /* Whether both hold the same pixels. */
    bool operator==(const Mask &other) const;
    /* The pixels of either mask / the pixels of this one that are not in `other`. */
    Mask united(const Mask &other) const;
    Mask subtracted(const Mask &other) const;

    /* Calls `visit(x, y)` for every pixel, row by row. */
    void forEach(const std::function<void(int, int)> &visit) const;

  private:
    /* A mask over `area` whose pixel (x, y) is in when `in(x, y)`; trimmed. */
    static Mask build(Rect area, const std::function<bool(int, int)> &in);

    Rect bounds_ = {0, 0, 0, 0};
    int count_ = 0;
    std::vector<unsigned char> bits_; /* bounds_.w * bounds_.h, row-major: 1 = in */
};

/* The mask of the pixels of `cel` that a flood fill from (x, y) with `options` would take (the
 * magic wand): the 4-connected region of the clicked color, or every pixel of it, within a
 * tolerance. Empty for a click outside the cel. */
Mask mask_of_region(const Cel &cel, int x, int y, const FillOptions &options);

/* The mask of a closed polygon through `points` (outline and inside, even-odd) within a width x
 * height cel (the lasso); points may lie outside it. */
Mask mask_of_polygon(int width, int height, const std::vector<std::pair<int, int>> &points);

#endif
