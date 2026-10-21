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
 *   FRAM  the duration of every frame in milliseconds (u16 each); optional, written only when a
 *         frame has other than FRAME_DURATION_DEFAULT, and before the first LAYR
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

/* Writes `data` to `path` through a temporary file next to it and a rename, so a failed write never
 * destroys an existing file. Returns false (leaving nothing behind) if it cannot be written. */
bool file_write_atomic(const std::string &path, const std::string &data);

/* Palette files. A .hex file has one color per line as RRGGBB or RRGGBBAA (an optional leading # is
 * accepted); a .gpl file is the GIMP palette format, which has no alpha: an export drops it. The
 * decoder tells the two apart by the "GIMP Palette" header, ignores blank lines and keeps the first
 * PALETTE_MAX_COLORS colors. It returns nothing for text without a valid color or with an invalid
 * line. */
std::string palette_encode_hex(const Palette &palette);
std::string palette_encode_gpl(const Palette &palette, const std::string &name);
std::optional<Palette> palette_decode(const std::string &text);

/* Save / load by file extension (.hex or .gpl). Return false / nothing for another extension or an
 * unwritable / unreadable / invalid file. */
bool palette_save(const Palette &palette, const std::string &path);
std::optional<Palette> palette_load(const std::string &path);

inline constexpr int IMAGE_EXPORT_MAX_SCALE = 32;
inline constexpr int IMAGE_EXPORT_MAX_SIZE = 16384;

/* Writes the flattened `frame` (visible layers, as displayed) as an image whose format follows the
 * extension of `path`: .png, .bmp and .tga keep the alpha channel, .jpg / .jpeg (lossy, no alpha)
 * shows the transparent parts on white. Returns false for another extension, a bad frame or an
 * unwritable file. `scale` (1..IMAGE_EXPORT_MAX_SCALE) enlarges the image by whole pixels (nearest
 * neighbor, every cel becomes a scale x scale block); a scale outside that range, or one that makes
 * the image wider or taller than IMAGE_EXPORT_MAX_SIZE, fails. */
bool image_export(const Sprite &sprite, int frame, const std::string &path, int scale = 1);

/* Reads any format image_export writes (and the other formats stb_image knows) as a sprite with the
 * image as its single layer and an empty palette. Returns nothing if the file is not a readable
 * image or exceeds CEL_MAX_DIMENSION. */
std::optional<Sprite> image_import(const std::string &path);

#endif
