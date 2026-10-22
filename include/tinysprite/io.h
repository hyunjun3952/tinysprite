#ifndef TINYSPRITE_IO_H
#define TINYSPRITE_IO_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "tinysprite/playback.h"
#include "tinysprite/rect.h"
#include "tinysprite/sprite.h"

/* The .pxa file format: a magic number and a version, then chunks (4-byte tag, 4-byte little-endian
 * length, payload). A reader skips tags it does not know, so later versions can add chunks.
 *   HEAD  width, height, frame count (u32 each); must come first
 *   FRAM  the duration of every frame in milliseconds (u16 each); optional, written only when a
 *         frame has other than FRAME_DURATION_DEFAULT, and before the first LAYR
 *   LINK  a layer index (u32), a pair count (u32), then (frame, source) pairs of u32: the frame
 *         shows the same cel as the earlier frame `source` (a linked cel); optional, one chunk per
 *         layer that has links, after the layers
 *   TAGS  tag count (u16), then per tag: name length (u8) and name, first and last frame (u32
 *         each, both included) and direction (u8: 0 forward, 1 reverse, 2 ping-pong); optional,
 *         written after the layers only when the sprite has tags
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

/* The extensions above for a message, e.g. ".gpl or .hex". */
std::string palette_formats();

inline constexpr int IMAGE_EXPORT_MAX_SCALE = 32;
inline constexpr int IMAGE_EXPORT_MAX_SIZE = 16384;

/* Writes the flattened `frame` (visible layers, as displayed) as an image whose format follows the
 * extension of `path`: .png, .bmp and .tga keep the alpha channel, .jpg / .jpeg (lossy, no alpha)
 * shows the transparent parts on white. Returns false for another extension, a bad frame or an
 * unwritable file. `scale` (1..IMAGE_EXPORT_MAX_SCALE) enlarges the image by whole pixels (nearest
 * neighbor, every cel becomes a scale x scale block); a scale outside that range, or one that makes
 * the image wider or taller than IMAGE_EXPORT_MAX_SIZE, fails. */
bool image_export(const Sprite &sprite, int frame, const std::string &path, int scale = 1);

/* The extensions image_export writes for a message, ".png, .bmp, .tga or .jpg" (.jpeg is the same
 * as .jpg and not listed). */
std::string image_write_formats();

/* Writes width x height pixels (row-major) as .png, .bmp, .tga or .jpg by the extension of `path`;
 * a jpg is flattened onto white in place. Returns false for another extension or an unwritable
 * file. image_export and the sprite sheet export use it; the UI uses it for screenshots. */
bool image_write_rgba(const std::string &path, int width, int height, std::vector<Rgba> &pixels);

/* Animated GIF export. The frames first..last (last = -1 for the final frame) are written in the
 * order `direction` plays them (a ping-pong goes out and back, its ends shown once), each for its
 * own duration, rounded to 1/100 s and at least 2/100 s since viewers treat shorter delays as
 * slow. A GIF has at most 256 colors and no partial transparency: a pixel at least half opaque is
 * shown in its color, any other is transparent; when the sprite uses more colors than fit they are
 * reduced to the closest palette (median cut). `scale` enlarges by whole pixels like image_export.
 * The animation loops forever unless `loop` is false. */
struct GifOptions {
    int first = 0;
    int last = -1;
    PlayMode direction = PlayMode::Forward;
    int scale = 1;
    bool loop = true;
};

/* The file's bytes, or nothing (an empty vector) for a bad range or scale. */
std::vector<uint8_t> gif_encode(const Sprite &sprite, const GifOptions &options);

/* Writes it to `path`, which must end in .gif. Returns false for a bad option or an unwritable
 * file. */
bool gif_export(const Sprite &sprite, const std::string &path, const GifOptions &options);

/* Sprite sheets: the frames first..last (last = -1 for the final frame), flattened as displayed and
 * laid out in one image. Horizontal puts them side by side, vertical one under the other, a grid
 * wraps after `columns` frames (0: as many as makes it roughly square). `spacing` transparent
 * pixels separate neighbouring frames (not the edges), `scale` enlarges by whole pixels like
 * image_export. */
enum class SheetLayout { Horizontal, Vertical, Grid };

struct SheetOptions {
    int first = 0;
    int last = -1;
    SheetLayout layout = SheetLayout::Horizontal;
    int columns = 0;
    int spacing = 0;
    int scale = 1;
};

struct SpriteSheet {
    int width = 0;
    int height = 0;
    std::vector<Rgba> pixels; /* width * height, row-major */
    std::vector<Rect> frames; /* where each frame went, in frame order */
};

/* Nothing for a bad range, scale, spacing or column count, or a sheet wider or taller than
 * IMAGE_EXPORT_MAX_SIZE. */
std::optional<SpriteSheet> spritesheet_render(const Sprite &sprite, const SheetOptions &options);

/* Writes the sheet as an image whose format follows the extension of `path` (as image_export).
 * Returns false for a bad option, another extension or an unwritable file. */
bool spritesheet_export(const Sprite &sprite, const std::string &path, const SheetOptions &options);

/* Reads any format image_export writes (and the other formats stb_image knows) as a sprite with the
 * image as its single layer and an empty palette. An animated GIF becomes one frame per GIF frame
 * (each with its delay as the duration). Returns nothing if the file is not a readable
 * image or exceeds CEL_MAX_DIMENSION. */
std::optional<Sprite> image_import(const std::string &path);

#endif
