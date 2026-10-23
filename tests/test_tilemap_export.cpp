#include <filesystem>
#include <fstream>
#include <sstream>

#include "minitest.hpp"
#include "tinysprite/io.hpp"

static bool has(const std::string &text, const std::string &part)
{
    return text.find(part) != std::string::npos;
}

static Tileset tiles(const char *name, int count, int size)
{
    Cel cel(size * count, size);
    cel.set(0, 0, Rgba{255, 0, 0, 255});
    return must_have(tileset_from_cel(cel, Rect{0, 0, size * count, size}, size, size, name));
}

/* Two tilesets (3 tiles of 4 px, 2 tiles of 4 px) and two maps. */
static Sprite sample()
{
    Sprite s(8, 8);
    TileMap ground = tilemap_blank("ground", 0, 3, 2);
    ground.cells = {1, 2, 3, 0, 0, 1};
    TileMap props = tilemap_blank("props", 1, 2, 2);
    props.cells = {2, 0, 0, 1};
    props.visible = false;
    s.setTileData({tiles("terrain", 3, 4), tiles("props", 2, 4)}, {ground, props});
    return s;
}

TEST(the_map_json_describes_the_size_tilesets_and_layers)
{
    std::string json = tilemap_json(sample(), {"m_terrain.png", "m_props.png"});
    ASSERT_TRUE(has(json, "\"type\": \"map\""));
    ASSERT_TRUE(has(json, "\"orientation\": \"orthogonal\""));
    ASSERT_TRUE(has(json, "\"width\": 3,"));
    ASSERT_TRUE(has(json, "\"height\": 2,"));
    ASSERT_TRUE(has(json, "\"tilewidth\": 4,"));
    ASSERT_TRUE(has(json, "\"image\": \"m_terrain.png\""));
    ASSERT_TRUE(has(json, "\"name\": \"terrain\""));
    ASSERT_TRUE(has(json, "\"firstgid\": 1"));
    ASSERT_TRUE(has(json, "\"firstgid\": 4")); /* after the 3 tiles of the first tileset */
    ASSERT_TRUE(has(json, "\"columns\": 3"));
    ASSERT_TRUE(has(json, "\"tilecount\": 3"));
    ASSERT_TRUE(has(json, "\"imagewidth\": 12"));
}

TEST(layer_cells_become_global_tile_ids)
{
    std::string json = tilemap_json(sample(), {"a.png", "b.png"});
    ASSERT_TRUE(has(json, "\"data\": [1, 2, 3, 0, 0, 1]"));
    /* props use the second tileset (firstgid 4); cell 2 is its tile 1 -> gid 5, padded to 3 x 2 */
    ASSERT_TRUE(has(json, "\"data\": [5, 0, 0, 0, 4, 0]"));
}

TEST(layers_keep_their_name_and_visibility)
{
    std::string json = tilemap_json(sample(), {"a.png", "b.png"});
    ASSERT_TRUE(has(json, "\"name\": \"ground\""));
    ASSERT_TRUE(has(json, "\"name\": \"props\""));
    ASSERT_TRUE(has(json, "\"visible\": true"));
    ASSERT_TRUE(has(json, "\"visible\": false"));
    ASSERT_TRUE(has(json, "\"type\": \"tilelayer\""));
}

TEST(names_are_escaped)
{
    Sprite s(8, 8);
    s.setTileData({tiles("a\"b", 1, 2)}, {tilemap_blank("l\\1", 0, 1, 1)});
    std::string json = tilemap_json(s, {"x.png"});
    ASSERT_TRUE(has(json, "a\\\"b"));
    ASSERT_TRUE(has(json, "l\\\\1"));
}

TEST(image_names_follow_the_file_stem_and_stay_distinct)
{
    Sprite s(8, 8);
    s.setTileData({tiles("Ground Tiles", 1, 2), tiles("Ground Tiles", 1, 2), tiles("", 1, 2)}, {});
    std::vector<std::string> names = tilemap_image_names(s, "level");
    ASSERT_EQ_INT(3, names.size());
    ASSERT_TRUE(names[0] == "level_Ground_Tiles.png");
    ASSERT_TRUE(names[1] == "level_Ground_Tiles_2.png");
    ASSERT_TRUE(names[2] == "level_tileset3.png");
}

TEST(exporting_writes_the_json_and_one_png_per_tileset)
{
    const std::string folder = "test_tilemap_export_dir";
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    ASSERT_TRUE(tilemap_export(sample(), folder + "/level.tmj"));
    ASSERT_TRUE(std::filesystem::exists(folder + "/level.tmj"));
    ASSERT_TRUE(std::filesystem::exists(folder + "/level_terrain.png"));
    ASSERT_TRUE(std::filesystem::exists(folder + "/level_props.png"));
    std::ifstream in(folder + "/level.tmj");
    std::stringstream text;
    text << in.rdbuf();
    ASSERT_TRUE(has(text.str(), "\"image\": \"level_terrain.png\""));
    std::optional<Sprite> image = image_import(folder + "/level_terrain.png");
    ASSERT_TRUE(image.has_value());
    if (image) {
        ASSERT_EQ_INT(12, image->width());
        ASSERT_EQ_INT(255, image->layer(0)->cel(0)->get(0, 0).r);
    }
    std::filesystem::remove_all(folder);
}

TEST(a_sprite_without_maps_cannot_be_exported)
{
    Sprite s(8, 8);
    ASSERT_TRUE(!tilemap_export(s, "never_written.tmj"));
    ASSERT_TRUE(!std::filesystem::exists("never_written.tmj"));
    s.setTileData({tiles("t", 1, 2)}, {});
    ASSERT_TRUE(!tilemap_export(s, "never_written.tmj"));
}

int main()
{
    RUN(the_map_json_describes_the_size_tilesets_and_layers);
    RUN(layer_cells_become_global_tile_ids);
    RUN(layers_keep_their_name_and_visibility);
    RUN(names_are_escaped);
    RUN(image_names_follow_the_file_stem_and_stay_distinct);
    RUN(exporting_writes_the_json_and_one_png_per_tileset);
    RUN(a_sprite_without_maps_cannot_be_exported);
    return TEST_SUMMARY();
}
