#ifndef TINYSPRITE_IO_IMAGE_WRITE_H
#define TINYSPRITE_IO_IMAGE_WRITE_H

#include <string>
#include <vector>

#include "tinysprite/palette.h"

/* Writes width x height pixels as .png, .bmp, .tga or .jpg (by the extension of `path`; a jpg is
 * flattened onto white in place). Returns false for another extension or an unwritable file. Shared
 * by the image and sprite sheet exports. */
bool image_write_rgba(const std::string &path, int width, int height, std::vector<Rgba> &pixels);

#endif
