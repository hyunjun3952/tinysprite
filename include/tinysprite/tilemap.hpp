#ifndef TINYSPRITE_TILEMAP_HPP
#define TINYSPRITE_TILEMAP_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "tinysprite/cel.hpp"
#include "tinysprite/rect.hpp"

/* Tilemaps: a tileset is a picture cut into equal tiles, a tilemap is a grid of references to
 * them. Both live on the Sprite next to the layers (Sprite::tilesets / tilemaps) and are edited
 * through tilemap_ops.hpp. */

inline constexpr int TILE_SIZE_MAX = 256;          /* pixels, each side of a tile */
inline constexpr int TILEMAP_MAX_DIMENSION = 1024; /* tiles, each side of a map */
inline constexpr int TILE_NAME_MAX = 63;           /* bytes */
inline constexpr int SPRITE_MAX_TILESETS = 64;
inline constexpr int SPRITE_MAX_TILEMAPS = 64;
inline constexpr int TILEMAP_OFFSET_MAX =
    CEL_MAX_DIMENSION; /* pixels, either way, of a map's origin */

/* The top three bits of a cell say how its tile is turned, as in Tiled: mirrored left-right,
 * mirrored top-bottom, and transposed (rows and columns swapped, only for square tiles). A tile is
 * transposed first, then mirrored left-right, then top-bottom. The rest of the cell is 0 for "no
 * tile" or the tile number + 1. */
inline constexpr uint32_t TILE_FLIP_X = 0x80000000u;
inline constexpr uint32_t TILE_FLIP_Y = 0x40000000u;
inline constexpr uint32_t TILE_TRANSPOSE = 0x20000000u;
inline constexpr uint32_t TILE_FLAGS = TILE_FLIP_X | TILE_FLIP_Y | TILE_TRANSPOSE;

/* The cell without its flags: 0 for no tile, else the tile number + 1. */
inline constexpr uint32_t tile_number(uint32_t cell)
{
    return cell & ~TILE_FLAGS;
}

/* The cell mirrored on screen left-right (or top-bottom) after whatever turn it already has. An
 * empty cell stays empty. */
uint32_t tile_flipped(uint32_t cell, bool horizontal);

/* The cell turned a quarter turn on screen, clockwise or not, after whatever turn it already has.
 * The result uses the transpose flag, so it is only valid for square tiles. An empty cell stays
 * empty. */
uint32_t tile_rotated(uint32_t cell, bool clockwise);

/* A picture (the atlas) cut into tileWidth x tileHeight tiles, numbered row by row from 0 at the
 * top-left. The atlas is shared, so copies of a tileset (history steps) are cheap. */
struct Tileset {
    std::string name;
    int tileWidth = 1;
    int tileHeight = 1;
    std::shared_ptr<const Cel> atlas;

    int columns() const;
    int rows() const;
    int tileCount() const
    {
        return columns() * rows();
    }
};

/* A columns x rows grid of tiles of one tileset (an index into Sprite::tilesets). A cell holds 0
 * for "no tile", or tile number + 1 with the turn flags above. The map is as big in pixels as its
 * grid times the tile size, and its top-left corner lies at (x, y) of the sprite. */
struct TileMap {
    std::string name;
    int tileset = 0;
    int columns = 1;
    int rows = 1;
    int x = 0; /* pixels, -TILEMAP_OFFSET_MAX..TILEMAP_OFFSET_MAX */
    int y = 0;
    bool visible = true;
    std::vector<uint32_t> cells; /* columns * rows, row-major */

    /* 0 for coordinates outside the grid. */
    uint32_t cell(int x, int y) const;
};

/* Whether `cell` can be put in a map of this tileset: empty, or a tile it has with any turn (the
 * transpose flag only for square tiles). */
bool tile_cell_valid(const Tileset &tileset, uint32_t cell);

/* The color at (x, y), counted from the top-left of the tile as it is drawn, of the tile `cell`
 * names with its turn applied. Transparent for an empty cell, a tile the tileset does not have, a
 * transposed cell of non-square tiles or a position outside the tile. */
Rgba tile_pixel(const Tileset &tileset, uint32_t cell, int x, int y);

/* Whether the tileset has a usable atlas whose size is a whole number of tiles of 1..TILE_SIZE_MAX
 * pixels. */
bool tileset_valid(const Tileset &tileset);

/* Whether the grid is 1..TILEMAP_MAX_DIMENSION tiles each way, has one cell per tile, refers to a
 * tileset of `tilesets` and only to tiles that exist in it (a turn flag needs a tile, and the
 * transpose flag square tiles), and the origin is within TILEMAP_OFFSET_MAX. */
bool tilemap_valid(const TileMap &map, const std::vector<Tileset> &tilesets);

/* Whether all tilesets and maps are valid together and within SPRITE_MAX_TILESETS /
 * SPRITE_MAX_TILEMAPS. */
bool tile_data_valid(const std::vector<Tileset> &tilesets, const std::vector<TileMap> &maps);

/* A map of empty cells. Throws std::invalid_argument for a grid outside 1..TILEMAP_MAX_DIMENSION.
 */
TileMap tilemap_blank(const std::string &name, int tileset, int columns, int rows);

/* Cuts `area` of `cel` (clipped to it) into tiles; whatever does not make a whole tile at the
 * right and bottom edges is left out. Returns nothing for a tile size outside 1..TILE_SIZE_MAX or
 * an area smaller than one tile. */
std::optional<Tileset> tileset_from_cel(const Cel &cel, Rect area, int tileWidth, int tileHeight,
                                        const std::string &name);

/* The map drawn as one picture of (columns x tile width) x (rows x tile height) pixels, every tile
 * turned as its cell says; empty cells stay transparent. `tileset` is the one the map refers to;
 * the map's own tileset index (its slot in the sprite) and its origin are not used. Returns
 * nothing if the grid is not valid or a cell names a tile the tileset does not have, or the
 * picture would be wider or taller than CEL_MAX_DIMENSION. */
std::optional<Cel> tilemap_render(const Tileset &tileset, const TileMap &map);

#endif
