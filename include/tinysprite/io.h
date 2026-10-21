#ifndef TINYSPRITE_IO_H
#define TINYSPRITE_IO_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "tinysprite/sprite.h"

/* The .pxa file format: a magic number and a version, then chunks (4-byte tag, 4-byte little-endian
 * length, payload). A reader skips tags it does not know, so later versions can add chunks.
 *   HEAD  width, height, frame count (u32 each); must come first
 *   PALT  color count (u16), then r, g, b, a per color
 *   LAYR  name length (u8) and name, visible (u8), opacity (u8), then the pixels of every frame
 *         (r, g, b, a, row-major); one chunk per layer, bottom to top */
inline constexpr uint32_t PXA_VERSION = 1;

std::vector<uint8_t> pxa_encode(const Sprite &sprite);

/* Returns nothing for data that is not a valid .pxa file of a version this build understands. */
std::optional<Sprite> pxa_decode(const std::vector<uint8_t> &data);

/* Return false / nothing if the file cannot be written / read or its content is invalid. */
bool pxa_save(const Sprite &sprite, const std::string &path);
std::optional<Sprite> pxa_load(const std::string &path);

/* Writes the flattened `frame` (visible layers, as displayed) as an image whose format follows the
 * extension of `path`: .png, .bmp and .tga keep the alpha channel, .jpg / .jpeg (lossy, no alpha)
 * shows the transparent parts on white. Returns false for another extension, a bad frame or an
 * unwritable file. */
bool image_export(const Sprite &sprite, int frame, const std::string &path);

/* Reads any format image_export writes (and the other formats stb_image knows) as a sprite with the
 * image as its single layer and an empty palette. Returns nothing if the file is not a readable
 * image or exceeds CEL_MAX_DIMENSION. */
std::optional<Sprite> image_import(const std::string &path);

#endif
