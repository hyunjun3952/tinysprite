#include <algorithm>
#include <filesystem>
#include <string>

#include "json_text.hpp"
#include "tinysprite/io.hpp"

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
        out += "    {\"columns\": " + number(ts.columns()) + ", \"firstgid\": " + number(firstgid) +
               ", \"image\": " + json_quoted(i < images.size() ? images[i] : "") +
               ", \"imageheight\": " + number(ts.atlas->height()) +
               ", \"imagewidth\": " + number(ts.atlas->width()) +
               ", \"margin\": 0, \"name\": " + json_quoted(ts.name) +
               ", \"spacing\": 0, \"tilecount\": " + number(ts.tileCount()) +
               ", \"tileheight\": " + number(ts.tileHeight) +
               ", \"tilewidth\": " + number(ts.tileWidth) + "}";
        firstgid += ts.tileCount();
    }
    return out;
}

/* The data array of a layer padded to width x height: global tile ids, row by row. */
std::string dataJson(const TileMap &map, const std::vector<int> &firstgids, int width, int height)
{
    std::string out;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t cell = map.cell(x, y);
            long long gid = cell == 0 ? 0 : firstgids[static_cast<size_t>(map.tileset)] + cell - 1;
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
        out += "    {\"data\": " + dataJson(map, firstgids, width, height) +
               ", \"height\": " + number(height) + ", \"id\": " + number(id) +
               ", \"name\": " + json_quoted(map.name) + ", \"opacity\": 1" +
               ", \"type\": \"tilelayer\", \"visible\": " + (map.visible ? "true" : "false") +
               ", \"width\": " + number(width) + ", \"x\": 0, \"y\": 0}";
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
    const Tileset *first = sprite.tilesets().empty() ? nullptr : &sprite.tilesets()[0];
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
        std::string name = stem + "_" + base + ".png";
        for (int copy = 2; std::find(names.begin(), names.end(), name) != names.end(); copy++) {
            name = stem + "_" + base + "_" + number(copy) + ".png";
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
