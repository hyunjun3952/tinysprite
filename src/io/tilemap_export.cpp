#include <algorithm>
#include <filesystem>
#include <string>

#include "tinysprite/io.hpp"
#include "util.hpp"

namespace
{

std::string safeName(const std::string &name)
{
    std::string out;
    for (char c : name) {
        bool plain = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     c == '-' || c == '_';
        out += plain ? c : '_';
    }
    return out;
}

/* "<stem>_<base>.png", or "<stem>_<base>_<copy>.png" from the second copy on. */
std::string imageName(const std::string &stem, const std::string &base, int copy)
{
    std::string name = stem;
    name += '_';
    name += base;
    if (copy > 1) {
        name += '_';
        name += std::to_string(copy);
    }
    return name + ".png";
}

std::string number(long long value)
{
    return std::to_string(value);
}

/* The "tilesets" entries, one line each. */
std::string tilesetsJson(const Sprite &sprite, const std::vector<std::string> &images)
{
    std::string out;
    int firstgid = 1;
    for (size_t i = 0; i < sprite.tilesets().size(); i++) {
        const Tileset &ts = sprite.tilesets()[i];
        out += i == 0 ? "\n" : ",\n";
        out += R"(    {"columns": )" + number(ts.columns()) + R"(, "firstgid": )" +
               number(firstgid) + R"(, "image": )" +
               json_quoted(i < images.size() ? images[i] : "") + R"(, "imageheight": )" +
               number(ts.atlas->height()) + R"(, "imagewidth": )" + number(ts.atlas->width()) +
               R"(, "margin": 0, "name": )" + json_quoted(ts.name) +
               R"(, "spacing": 0, "tilecount": )" + number(ts.tileCount()) + R"(, "tileheight": )" +
               number(ts.tileHeight) + R"(, "tilewidth": )" + number(ts.tileWidth) + "}";
        firstgid += ts.tileCount();
    }
    return out;
}

/* The data array of a layer padded to width x height: global tile ids, row by row. A turned tile
 * keeps its flip bits in the top of its id, where Tiled reads them. */
std::string dataJson(const TileMap &map, const std::vector<int> &firstgids, int width, int height)
{
    std::string out;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t cell = map.cell(x, y);
            long long gid = 0;
            if (tile_number(cell) != 0) {
                gid = (firstgids[static_cast<size_t>(map.tileset)] + tile_number(cell) - 1) |
                      (cell & TILE_FLAGS);
            }
            out += (out.empty() ? "" : ", ") + number(gid);
        }
    }
    return "[" + out + "]";
}

std::string layersJson(const Sprite &sprite, const std::vector<int> &firstgids, int width,
                       int height)
{
    std::string out;
    int id = 1;
    for (const TileMap &map : sprite.tilemaps()) {
        out += id == 1 ? "\n" : ",\n";
        out += R"(    {"data": )" + dataJson(map, firstgids, width, height) + R"(, "height": )" +
               number(height) + R"(, "id": )" + number(id) + R"(, "name": )" +
               json_quoted(map.name) + R"(, "offsetx": )" + number(map.x) + R"(, "offsety": )" +
               number(map.y) + R"(, "opacity": 1)" + R"(, "type": "tilelayer", "visible": )" +
               (map.visible ? "true" : "false") + R"(, "width": )" + number(width) +
               R"(, "x": 0, "y": 0})";
        id++;
    }
    return out;
}

} // namespace

std::string tilemap_json(const Sprite &sprite, const std::vector<std::string> &images)
{
    int width = 1;
    int height = 1;
    for (const TileMap &map : sprite.tilemaps()) {
        width = std::max(width, map.columns);
        height = std::max(height, map.rows);
    }
    std::vector<int> firstgids;
    int next = 1;
    for (const Tileset &ts : sprite.tilesets()) {
        firstgids.push_back(next);
        next += ts.tileCount();
    }
    const Tileset *first = sprite.tilesets().empty() ? nullptr : sprite.tilesets().data();
    std::string layers = layersJson(sprite, firstgids, width, height);
    return "{\n  \"compressionlevel\": -1,\n  \"height\": " + number(height) +
           ",\n  \"infinite\": false,\n  \"layers\": [" + layers + "\n  ],\n  \"nextlayerid\": " +
           number(static_cast<long long>(sprite.tilemaps().size()) + 1) +
           ",\n  \"nextobjectid\": 1,\n  \"orientation\": \"orthogonal\",\n"
           "  \"renderorder\": \"right-down\",\n  \"tiledversion\": \"1.10.2\",\n"
           "  \"tileheight\": " +
           number(first ? first->tileHeight : 1) + ",\n  \"tilesets\": [" +
           tilesetsJson(sprite, images) +
           "\n  ],\n  \"tilewidth\": " + number(first ? first->tileWidth : 1) +
           ",\n  \"type\": \"map\",\n  \"version\": \"1.10\",\n  \"width\": " + number(width) +
           "\n}\n";
}

std::vector<std::string> tilemap_image_names(const Sprite &sprite, const std::string &stem)
{
    std::vector<std::string> names;
    for (size_t i = 0; i < sprite.tilesets().size(); i++) {
        std::string base = safeName(sprite.tilesets()[i].name);
        if (base.empty()) {
            base = "tileset" + number(static_cast<long long>(i) + 1);
        }
        std::string name = imageName(stem, base, 1);
        for (int copy = 2; std::find(names.begin(), names.end(), name) != names.end(); copy++) {
            name = imageName(stem, base, copy);
        }
        names.push_back(name);
    }
    return names;
}

bool tilemap_export(const Sprite &sprite, const std::string &path)
{
    if (sprite.tilemaps().empty()) {
        return false;
    }
    const std::filesystem::path file = path_from_utf8(path);
    const std::vector<std::string> images = tilemap_image_names(sprite, path_to_utf8(file.stem()));
    for (size_t i = 0; i < images.size(); i++) {
        const Cel &atlas = *sprite.tilesets()[i].atlas;
        std::vector<Rgba> pixels(
            atlas.pixels(), atlas.pixels() + static_cast<size_t>(atlas.width()) * atlas.height());
        std::string target = path_to_utf8(file.parent_path() / path_from_utf8(images[i]));
        if (!image_write_rgba(target, atlas.width(), atlas.height(), pixels)) {
            return false;
        }
    }
    return file_write_atomic(path, tilemap_json(sprite, images));
}
