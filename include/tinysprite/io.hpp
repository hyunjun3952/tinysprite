#ifndef TINYSPRITE_IO_HPP
#define TINYSPRITE_IO_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "tinysprite/playback.hpp"
#include "tinysprite/rect.hpp"
#include "tinysprite/sprite.hpp"

/* Paths cross the API as UTF-8 strings. On Windows the narrow file APIs read a string in the ANSI
 * code page instead, so every path is turned into a std::filesystem::path through this before it
 * is opened, and a file name read from a folder is turned back with path_to_utf8. Elsewhere both
 * are plain copies. */
std::filesystem::path path_from_utf8(const std::string &path);
std::string path_to_utf8(const std::filesystem::path &path);

/*
 * The .pxa file format: a magic number and a version (PXA_VERSION), then chunks (4-byte tag, 4-byte
 * little-endian length, payload), written in the order below. A reader skips tags it does not know,
 * so later versions can add chunks, and rejects other versions and truncated data.
 *   HEAD  width, height, frame count (u32 each); must come first
 *   FRAM  the duration of every frame in milliseconds (u16 each); optional, written only when a
 *         frame has other than FRAME_DURATION_DEFAULT; must come before the first LAYR
 *   PALT  color count (u16), then r, g, b, a per color
 *   LAYR  name length (u8) and name, flags (u8: bit 0 visible, bit 1 locked), opacity (u8), then
 *         the pixels of every frame (r, g, b, a, row-major); one chunk per layer, bottom to top
 *   LINK  a layer index (u32), a pair count (u32), then (frame, source) pairs of u32: the frame
 *         shows the same cel as the earlier frame `source` (a linked cel); optional, one chunk per
 *         layer that has links
 *   BLND  entry count (u32), then per entry a layer index (u32) and a blend mode (u8, BlendMode);
 *         optional, lists only the layers that are not Normal; a mode this version does not know
 *         reads as Normal
 *   TAGS  tag count (u16), then per tag: name length (u8) and name, first and last frame (u32
 *         each, both included) and direction (u8: 0 forward, 1 reverse, 2 ping-pong); optional
 *   SLCE  slice count (u16), then per slice: name length (u8) and name, x, y, width and height (u32
 *         each) and flags (u8: 0 no pivot, 1 a pivot follows as two u32, in cels from the slice's
 *         top-left corner; any other value makes the file invalid); optional. A slice that does
 *         not fit the sprite is dropped on reading and the pixels still load
 *   TSET  tileset count (u16), then per tileset: name length (u8) and name, tile width, tile
 *         height, atlas width and atlas height (u32 each), then the atlas pixels (r, g, b, a);
 *         optional
 *   TMAP  map count (u16), then per map: name length (u8) and name, tileset index, columns and rows
 *         (u32 each), visible (u8) and one u32 per cell (0: no tile, else tile number + 1);
 *         optional. The reader keeps the tile data only if every tileset and map is valid together,
 *         else it drops it and the pixels still load
 */
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
bool file_write_atomic(const std::string &path, const uint8_t *data, size_t size);

/* Why the last file_write_atomic on this thread failed, as the system words it ("No such file or
 * directory"); empty if it succeeded. */
std::string file_write_error();

/*
 * Palette files. A .hex file has one color per line as RRGGBB or RRGGBBAA (an optional leading # is
 * accepted; a line starting with ; or // is a comment). A .gpl file is the GIMP palette format,
 * which has no alpha (an export drops it): the "GIMP Palette" header, then "R G B name" lines, with
 * # comments and the Name: and Columns: lines skipped. The decoder tells the two apart by the
 * header, ignores a UTF-8 byte order mark and blank lines, and keeps the first PALETTE_MAX_COLORS
 * colors. It returns nothing for text without a valid color or with an invalid line.
 */
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
 * The animation loops forever unless `loop` is false (a single frame has nothing to loop). */
struct GifOptions {
    int first = 0;
    int last = -1;
    PlayMode direction = PlayMode::Forward;
    int scale = 1;
    bool loop = true;
};

/* How many images a GIF holds, counted from its block structure without decoding any pixel; 0 for
 * data that is not a well-formed GIF. image_import uses it to refuse an animation whose frames
 * would not fit in memory before stb_image decodes them all. */
int gif_frame_count(const std::vector<uint8_t> &data);

/* The file's bytes, or nothing (an empty vector) for a bad range or scale. */
std::vector<uint8_t> gif_encode(const Sprite &sprite, const GifOptions &options);

/* Writes it to `path`, which must end in .gif. Returns false for a bad option or an unwritable
 * file. */
bool gif_export(const Sprite &sprite, const std::string &path, const GifOptions &options);

/* Sprite sheets: the frames first..last (last = -1 for the final frame), flattened as displayed and
 * laid out in one image. Horizontal puts them side by side, vertical one under the other, a grid
 * wraps after `columns` frames (0: as many as makes it roughly square). `spacing` transparent
 * pixels separate neighbouring frames (not the edges), `scale` enlarges by whole pixels like
 * image_export. Packed trims every frame to the pixels that are drawn (a frame with none keeps a
 * 1 x 1 transparent cell) and packs the trimmed pieces tightly into the smallest sheet it finds
 * (see pack_rects); `spacing` is the gap between pieces, `columns` is not used, and the JSON tells
 * where each piece sat in its frame. */
enum class SheetLayout { Horizontal, Vertical, Grid, Packed };

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
    /* A packed sheet only: the pixels of each frame are the part of the frame that is drawn, and
     * trims[i] says which part, as an x, y, w, h inside the frame as drawn (the scale applied; w
     * and h equal frames[i]'s). Empty for the other layouts. */
    bool trimmed = false;
    std::vector<Rect> trims;
};

/* Nothing for a bad range, scale, spacing or column count, or a sheet wider or taller than
 * IMAGE_EXPORT_MAX_SIZE. */
std::optional<SpriteSheet> spritesheet_render(const Sprite &sprite, const SheetOptions &options);

/* Writes the sheet as an image whose format follows the extension of `path` (as image_export).
 * Returns false for a bad option, another extension or an unwritable file. */
bool spritesheet_export(const Sprite &sprite, const std::string &path, const SheetOptions &options);

/* The metadata a game loads next to the sheet image, as JSON: "meta" (app, the image's file name
 * `image`, the sheet's size, the size of one frame as drawn, the scale), "frames" (one entry per
 * exported frame: its rect in the sheet and its duration in milliseconds), "tags" (name, first
 * and last frame, direction "forward" / "reverse" / "pingpong") and "slices" (name, "bounds" in the
 * frame as drawn, and an optional "pivot": a point in pixels from the slice's top-left corner, both
 * multiplied by the scale). Frame numbers count from the first exported frame; a tag is clamped to
 * the exported range and dropped when it is outside. */
std::string spritesheet_metadata_json(const Sprite &sprite, const SheetOptions &options,
                                      const SpriteSheet &sheet, const std::string &image);

/* spritesheet_export plus the metadata written to `jsonPath` (its "image" is the file name of
 * `path`). Returns false if either file cannot be written; the image is written first. */
bool spritesheet_export_with_json(const Sprite &sprite, const std::string &path,
                                  const std::string &jsonPath, const SheetOptions &options);

/* Tilemap export in the JSON map format of the Tiled editor (.tmj), which game engines and
 * loaders already read. Every tileset of the sprite is listed once with consecutive global tile
 * ids (`firstgid` 1, then after the tiles of the one before), and every tilemap becomes a
 * "tilelayer" with its name and visibility. The map is as big as the biggest layer, smaller layers
 * are padded with empty cells, and its tile size is the first tileset's. A cell of 0 stays 0, any
 * other becomes firstgid + tile number. `images` has the file name of each tileset's image, in
 * order. */
std::string tilemap_json(const Sprite &sprite, const std::vector<std::string> &images);

/* The image file name of every tileset for a map file called `stem` ("level" -> "level_<tileset
 * name>.png", with characters other than letters, digits, '-' and '_' turned into '_'; a tileset
 * without a name is "tileset<N>"; a name used twice gets "_<N>" from its second use on). */
std::vector<std::string> tilemap_image_names(const Sprite &sprite, const std::string &stem);

/* Writes the map JSON to `path` and the atlas of each tileset as a PNG beside it, named by
 * tilemap_image_names. Returns false if the sprite has no tilemap or a file cannot be written. */
bool tilemap_export(const Sprite &sprite, const std::string &path);

/* Aseprite files (.aseprite / .ase) as sprites. Image layers become layers (groups are flattened: a
 * layer inside a hidden group is hidden, tilemap layers are left out), cels keep their place,
 * opacity and links, indexed and grayscale files become RGBA, and the frame durations, tags,
 * palette and slices (the first key of each) come along. Layer opacity, visibility, lock and the
 * blend modes this editor has are kept; other blend modes show as Normal. Returns nothing for data
 * that is not a valid Aseprite file or does not fit the limits (CEL_MAX_DIMENSION, the cel budget;
 * frames beyond SPRITE_MAX_FRAMES are dropped). aseprite_load recognizes the file by its content
 * and reads it only then, so any path may be tried. */
std::optional<Sprite> aseprite_decode(const std::vector<uint8_t> &data);
std::optional<Sprite> aseprite_load(const std::string &path);

/* Reads any format image_export writes (and the other formats stb_image knows) as a sprite with the
 * image as its single layer and an empty palette. An animated GIF becomes one frame per GIF frame
 * (each with its delay as the duration; at most SPRITE_MAX_FRAMES), and an Aseprite file is read by
 * aseprite_load. Returns
 * nothing if the file is not a readable image or exceeds CEL_MAX_DIMENSION. */
std::optional<Sprite> image_import(const std::string &path);

#endif
