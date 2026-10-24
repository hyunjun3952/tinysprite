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
 * for "no tile", or tile number + 1. The map is as big in pixels as its grid times the tile size.
 */
struct TileMap {
    std::string name;
    int tileset = 0;
    int columns = 1;
    int rows = 1;
    bool visible = true;
    std::vector<uint32_t> cells; /* columns * rows, row-major */

    /* 0 for coordinates outside the grid. */
    uint32_t cell(int x, int y) const;
};

/* Whether the tileset has a usable atlas whose size is a whole number of tiles of 1..TILE_SIZE_MAX
 * pixels. */
bool tileset_valid(const Tileset &tileset);

/* Whether the grid is 1..TILEMAP_MAX_DIMENSION tiles each way, has one cell per tile, refers to a
 * tileset of `tilesets` and only to tiles that exist in it. */
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

/* The map drawn as one picture of (columns x tile width) x (rows x tile height) pixels; empty
 * cells stay transparent. `tileset` is the one the map refers to; the map's own tileset index (its
 * slot in the sprite) is not used. Returns nothing if the grid is not valid or a cell names a tile
 * the tileset does not have, or the picture would be wider or taller than CEL_MAX_DIMENSION. */
std::optional<Cel> tilemap_render(const Tileset &tileset, const TileMap &map);

#endif
