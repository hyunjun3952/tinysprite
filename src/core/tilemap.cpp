#include "tinysprite/tilemap.hpp"

#include <algorithm>
#include <stdexcept>

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

} // namespace

bool tileset_valid(const Tileset &tileset)
{
    return tileset.atlas && tileSizeOk(tileset.tileWidth) && tileSizeOk(tileset.tileHeight) &&
           tileset.atlas->width() % tileset.tileWidth == 0 &&
           tileset.atlas->height() % tileset.tileHeight == 0;
}

bool tilemap_valid(const TileMap &map, const std::vector<Tileset> &tilesets)
{
    if (!gridOk(map.columns) || !gridOk(map.rows) || map.tileset < 0 ||
        map.tileset >= static_cast<int>(tilesets.size()) ||
        map.cells.size() != static_cast<size_t>(map.columns) * static_cast<size_t>(map.rows)) {
        return false;
    }
    auto tiles = static_cast<uint32_t>(tilesets[static_cast<size_t>(map.tileset)].tileCount());
    return std::all_of(map.cells.begin(), map.cells.end(),
                       [tiles](uint32_t cell) { return cell <= tiles; });
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

/* Copies tile number `tile` of the tileset to the picture, its top-left corner at (x, y). */
void drawTile(Cel &picture, const Tileset &tileset, int tile, int x, int y)
{
    int sx = tile % tileset.columns() * tileset.tileWidth;
    int sy = tile / tileset.columns() * tileset.tileHeight;
    for (int dy = 0; dy < tileset.tileHeight; dy++) {
        for (int dx = 0; dx < tileset.tileWidth; dx++) {
            picture.set(x + dx, y + dy, tileset.atlas->get(sx + dx, sy + dy));
        }
    }
}

} // namespace

std::optional<Cel> tilemap_render(const Tileset &tileset, const TileMap &map)
{
    if (!tileset_valid(tileset) || !tilemap_valid(map, {tileset})) {
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
                drawTile(picture, tileset, static_cast<int>(cell) - 1, cx * tileset.tileWidth,
                         cy * tileset.tileHeight);
            }
        }
    }
    return picture;
}
