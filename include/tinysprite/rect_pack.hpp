#ifndef TINYSPRITE_RECT_PACK_HPP
#define TINYSPRITE_RECT_PACK_HPP

#include <optional>
#include <vector>

#include "tinysprite/rect.hpp"

/* Rectangle packing for sprite sheets. */

/* Where each rectangle went on a sheet of `width` x `height`. */
struct PackedRects {
    int width = 0;
    int height = 0;
    std::vector<Rect> rects; /* one per input, in input order; the size is the input's */
};

/* Places rectangles of the sizes given (the w and h of each `sizes` entry; x and y are ignored) on
 * one sheet with the skyline bottom-left method: tall ones first, each at the lowest, then leftmost
 * spot where it fits. The sheet width is chosen (tried from about the square root of the total
 * area upward) for the smallest sheet; `padding` empty pixels are kept between neighbouring
 * rectangles (not at the sheet edges). The result does not depend on anything but the input.
 * Returns nothing for no rectangles, one with a side below 1, a negative padding, or when they do
 * not fit on a sheet of at most `maxSize` x `maxSize`. */
std::optional<PackedRects> pack_rects(const std::vector<Rect> &sizes, int padding, int maxSize);

#endif
