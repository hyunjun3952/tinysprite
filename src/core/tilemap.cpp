#include "tinysprite/tilemap.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <utility>

#include "tinysprite/layer.hpp"

int Tileset::columns() const
{
    return atlas ? atlas->width() / std::max(tileWidth, 1) : 0;
}

int Tileset::rows() const
{
    return atlas ? atlas->height() / std::max(tileHeight, 1) : 0;
}

uint32_t TileMap::cell(int x, int y) const
{
    if (x < 0 || y < 0 || x >= columns || y >= rows) {
        return 0;
    }
    return cells[static_cast<size_t>(y) * static_cast<size_t>(columns) + static_cast<size_t>(x)];
}

namespace
{

bool tileSizeOk(int size)
{
    return size >= 1 && size <= TILE_SIZE_MAX;
}

bool gridOk(int size)
{
    return size >= 1 && size <= TILEMAP_MAX_DIMENSION;
}

/* Whether `cell` is empty, or a tile of a tileset with `tileCount` tiles with any turn (the
 * transpose flag only for square tiles). A turn without a tile is not a cell. */
bool cellFits(uint32_t cell, uint32_t tileCount, bool square)
{
    const uint32_t number = tile_number(cell);
    if (number > tileCount || (number == 0 && cell != 0)) {
        return false;
    }
    return square || (cell & TILE_TRANSPOSE) == 0;
}

/* Whether the grid is a legal size with one cell per tile, each cell fitting a tileset with
 * `tileCount` tiles. Does not look at which tileset the map refers to. */
bool gridFits(const TileMap &map, int tileCount, bool square)
{
    return gridOk(map.columns) && gridOk(map.rows) &&
           map.cells.size() == static_cast<size_t>(map.columns) * static_cast<size_t>(map.rows) &&
           std::abs(map.x) <= TILEMAP_OFFSET_MAX && std::abs(map.y) <= TILEMAP_OFFSET_MAX &&
           std::all_of(map.cells.begin(), map.cells.end(),
                       [tiles = static_cast<uint32_t>(tileCount), square](uint32_t cell) {
                           return cellFits(cell, tiles, square);
                       });
}

/* A 2 x 2 integer matrix, row by row. */
struct Matrix {
    int a, b, c, d;

    bool operator==(const Matrix &other) const
    {
        return a == other.a && b == other.b && c == other.c && d == other.d;
    }
};

Matrix times(const Matrix &left, const Matrix &right)
{
    return Matrix{left.a * right.a + left.b * right.c, left.a * right.b + left.b * right.d,
                  left.c * right.a + left.d * right.c, left.c * right.b + left.d * right.d};
}

/* What the flags do to a point of the tile (centered, y pointing down): the transpose first, then
 * the left-right mirror, then the top-bottom one. */
Matrix turnOf(uint32_t cell)
{
    Matrix turn{1, 0, 0, 1};
    if ((cell & TILE_TRANSPOSE) != 0) {
        turn = times(Matrix{0, 1, 1, 0}, turn);
    }
    if ((cell & TILE_FLIP_X) != 0) {
        turn = times(Matrix{-1, 0, 0, 1}, turn);
    }
    if ((cell & TILE_FLIP_Y) != 0) {
        turn = times(Matrix{1, 0, 0, -1}, turn);
    }
    return turn;
}

/* `cell` with the flags that make exactly `turn`; the eight combinations are the eight turns. */
uint32_t withTurn(uint32_t cell, const Matrix &turn)
{
    for (uint32_t flags = 0; flags < 8; flags++) {
        const uint32_t candidate = tile_number(cell) | (flags << 29);
        if (turnOf(candidate) == turn) {
            return candidate;
        }
    }
    return cell;
}

/* The cell with `step` applied on screen after its own turn; an empty cell stays empty. */
uint32_t turned(uint32_t cell, const Matrix &step)
{
    return tile_number(cell) == 0 ? cell : withTurn(cell, times(step, turnOf(cell)));
}

} // namespace

uint32_t tile_flipped(uint32_t cell, bool horizontal)
{
    return turned(cell, horizontal ? Matrix{-1, 0, 0, 1} : Matrix{1, 0, 0, -1});
}

uint32_t tile_rotated(uint32_t cell, bool clockwise)
{
    return turned(cell, clockwise ? Matrix{0, -1, 1, 0} : Matrix{0, 1, -1, 0});
}

bool tile_cell_valid(const Tileset &tileset, uint32_t cell)
{
    return cellFits(cell, static_cast<uint32_t>(tileset.tileCount()),
                    tileset.tileWidth == tileset.tileHeight);
}

Rgba tile_pixel(const Tileset &tileset, uint32_t cell, int x, int y)
{
    const uint32_t number = tile_number(cell);
    if (!tileset.atlas || number == 0 || x < 0 || y < 0 || x >= tileset.tileWidth ||
        y >= tileset.tileHeight || !tile_cell_valid(tileset, cell)) {
        return Rgba{};
    }
    int sx = (cell & TILE_FLIP_X) != 0 ? tileset.tileWidth - 1 - x : x;
    int sy = (cell & TILE_FLIP_Y) != 0 ? tileset.tileHeight - 1 - y : y;
    if ((cell & TILE_TRANSPOSE) != 0) {
        std::swap(sx, sy);
    }
    const int tile = static_cast<int>(number) - 1;
    return tileset.atlas->get(tile % tileset.columns() * tileset.tileWidth + sx,
                              tile / tileset.columns() * tileset.tileHeight + sy);
}

bool tileset_valid(const Tileset &tileset)
{
    return tileset.atlas && tileSizeOk(tileset.tileWidth) && tileSizeOk(tileset.tileHeight) &&
           tileset.atlas->width() % tileset.tileWidth == 0 &&
           tileset.atlas->height() % tileset.tileHeight == 0;
}

bool tilemap_valid(const TileMap &map, const std::vector<Tileset> &tilesets)
{
    if (map.tileset < 0 || map.tileset >= static_cast<int>(tilesets.size())) {
        return false;
    }
    const Tileset &tileset = tilesets[static_cast<size_t>(map.tileset)];
    return gridFits(map, tileset.tileCount(), tileset.tileWidth == tileset.tileHeight);
}

bool tile_data_valid(const std::vector<Tileset> &tilesets, const std::vector<TileMap> &maps)
{
    return tilesets.size() <= SPRITE_MAX_TILESETS && maps.size() <= SPRITE_MAX_TILEMAPS &&
           std::all_of(tilesets.begin(), tilesets.end(), tileset_valid) &&
           std::all_of(maps.begin(), maps.end(),
                       [&](const TileMap &map) { return tilemap_valid(map, tilesets); });
}

TileMap tilemap_blank(const std::string &name, int tileset, int columns, int rows)
{
    if (!gridOk(columns) || !gridOk(rows)) {
        throw std::invalid_argument("tilemap size out of range");
    }
    TileMap map;
    map.name = layer_name_truncate(name, TILE_NAME_MAX);
    map.tileset = tileset;
    map.columns = columns;
    map.rows = rows;
    map.cells.assign(static_cast<size_t>(columns) * static_cast<size_t>(rows), 0);
    return map;
}

std::optional<Tileset> tileset_from_cel(const Cel &cel, Rect area, int tileWidth, int tileHeight,
                                        const std::string &name)
{
    Rect r = rect_clip(area, cel.width(), cel.height());
    if (!tileSizeOk(tileWidth) || !tileSizeOk(tileHeight) || r.w < tileWidth || r.h < tileHeight) {
        return std::nullopt;
    }
    int width = r.w / tileWidth * tileWidth;
    int height = r.h / tileHeight * tileHeight;
    auto atlas = std::make_shared<Cel>(width, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            atlas->set(x, y, cel.get(r.x + x, r.y + y));
        }
    }
    Tileset tileset;
    tileset.name = layer_name_truncate(name, TILE_NAME_MAX);
    tileset.tileWidth = tileWidth;
    tileset.tileHeight = tileHeight;
    tileset.atlas = std::move(atlas);
    return tileset;
}

namespace
{

/* Copies the tile `cell` names, turned as the cell says, to the picture, its top-left corner at
 * (x, y). */
void drawTile(Cel &picture, const Tileset &tileset, uint32_t cell, int x, int y)
{
    for (int dy = 0; dy < tileset.tileHeight; dy++) {
        for (int dx = 0; dx < tileset.tileWidth; dx++) {
            picture.set(x + dx, y + dy, tile_pixel(tileset, cell, dx, dy));
        }
    }
}

} // namespace

std::optional<Cel> tilemap_render(const Tileset &tileset, const TileMap &map)
{
    if (!tileset_valid(tileset) ||
        !gridFits(map, tileset.tileCount(), tileset.tileWidth == tileset.tileHeight)) {
        return std::nullopt;
    }
    long long width = static_cast<long long>(map.columns) * tileset.tileWidth;
    long long height = static_cast<long long>(map.rows) * tileset.tileHeight;
    if (width > CEL_MAX_DIMENSION || height > CEL_MAX_DIMENSION) {
        return std::nullopt;
    }
    Cel picture(static_cast<int>(width), static_cast<int>(height));
    for (int cy = 0; cy < map.rows; cy++) {
        for (int cx = 0; cx < map.columns; cx++) {
            if (uint32_t cell = map.cell(cx, cy)) {
                drawTile(picture, tileset, cell, cx * tileset.tileWidth, cy * tileset.tileHeight);
            }
        }
    }
    return picture;
}
