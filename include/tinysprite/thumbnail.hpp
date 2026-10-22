#ifndef TINYSPRITE_THUMBNAIL_HPP
#define TINYSPRITE_THUMBNAIL_HPP

#include <vector>

#include "tinysprite/cel.hpp"

/* Size of a preview of a width x height image that fits in maxWidth x maxHeight with the same
 * aspect ratio: a whole-number factor when the image is smaller than the box (so pixel art stays
 * crisp), a fraction when it is larger, and never below 1 x 1. */
void thumbnail_size(int width, int height, int maxWidth, int maxHeight, int &outWidth,
                    int &outHeight);

/* A thumbWidth x thumbHeight row-major preview of `cel`: each preview pixel takes the color of the
 * cel pixel under its center (nearest), so the cost depends on the preview, not on the cel. The
 * pixels keep their alpha. */
std::vector<Rgba> thumbnail_pixels(const Cel &cel, int thumbWidth, int thumbHeight);

#endif
