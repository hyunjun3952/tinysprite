#include "minitest.hpp"
#include "tinysprite/io.hpp"

static Sprite with_tiles()
{
    Sprite s(8, 8);
    Cel cel(4, 2);
    cel.set(0, 0, Rgba{255, 0, 0, 255});
    cel.set(3, 1, Rgba{0, 0, 255, 128});
    std::vector<Tileset> sets = {
        must_have(tileset_from_cel(cel, Rect{0, 0, 4, 2}, 2, 2, "ground"))};
    TileMap map = tilemap_blank("level", 0, 3, 2);
    map.cells = {1, 0, 2, 2, 1, 0};
    map.visible = false;
    s.setTileData(sets, {map});
    return s;
}

TEST(tiles_survive_a_save_and_load)
{
    Sprite back = must_have(pxa_decode(pxa_encode(with_tiles())));
    ASSERT_EQ_INT(1, back.tilesets().size());
    const Tileset &ts = back.tilesets()[0];
    ASSERT_TRUE(ts.name == "ground");
    ASSERT_EQ_INT(2, ts.tileWidth);
    ASSERT_EQ_INT(2, ts.tileCount());
    ASSERT_EQ_INT(255, ts.atlas->get(0, 0).r);
    ASSERT_EQ_INT(128, ts.atlas->get(3, 1).a);
    ASSERT_EQ_INT(1, back.tilemaps().size());
    const TileMap &map = back.tilemaps()[0];
    ASSERT_TRUE(map.name == "level");
    ASSERT_TRUE(!map.visible);
    ASSERT_EQ_INT(3, map.columns);
    ASSERT_EQ_INT(2, map.cell(2, 0));
    ASSERT_EQ_INT(1, map.cell(1, 1));
}

/* Where the chunk named `tag` starts in an encoded file (the tag's first byte). */
static size_t chunk_at(const std::vector<uint8_t> &data, const char *tag)
{
    std::string text(data.begin(), data.end());
    return text.find(tag);
}

TEST(a_maps_position_and_turned_cells_survive_a_save_and_load)
{
    Sprite s = with_tiles();
    TileMap map = s.tilemaps()[0];
    map.x = -12;
    map.y = 30;
    map.cells[0] = 1 | TILE_FLIP_X | TILE_TRANSPOSE;
    ASSERT_TRUE(s.setTileData(s.tilesets(), {map}));
    Sprite back = must_have(pxa_decode(pxa_encode(s)));
    ASSERT_EQ_INT(-12, back.tilemaps()[0].x);
    ASSERT_EQ_INT(30, back.tilemaps()[0].y);
    ASSERT_EQ_INT(1 | TILE_FLIP_X | TILE_TRANSPOSE, back.tilemaps()[0].cell(0, 0));
}

TEST(maps_at_the_origin_write_no_position_chunk)
{
    std::vector<uint8_t> data = pxa_encode(with_tiles());
    std::string text(data.begin(), data.end());
    ASSERT_TRUE(text.find("TPOS") == std::string::npos);
}

TEST(a_position_out_of_range_is_dropped_and_the_tiles_stay)
{
    Sprite s = with_tiles();
    TileMap map = s.tilemaps()[0];
    map.x = 4;
    s.setTileData(s.tilesets(), {map});
    std::vector<uint8_t> data = pxa_encode(s);
    size_t at = chunk_at(data, "TPOS");
    ASSERT_TRUE(at != std::string::npos);
    /* tag, length, count (u16), then x as a u32: make it huge */
    for (size_t i = 0; i < 4; i++) {
        data[at + 4 + 4 + 2 + i] = 0x7F;
    }
    Sprite back = must_have(pxa_decode(data));
    ASSERT_EQ_INT(1, back.tilemaps().size());
    ASSERT_EQ_INT(0, back.tilemaps()[0].x);
}

TEST(a_sprite_without_tiles_writes_no_tile_chunks)
{
    Sprite s(4, 4);
    std::vector<uint8_t> data = pxa_encode(s);
    std::string text(data.begin(), data.end());
    ASSERT_TRUE(text.find("TSET") == std::string::npos);
    ASSERT_TRUE(text.find("TMAP") == std::string::npos);
}

TEST(a_map_that_points_at_a_missing_tileset_drops_the_tiles_but_keeps_the_pixels)
{
    Sprite s = with_tiles();
    s.layer(0)->cel(0)->set(1, 1, Rgba{9, 9, 9, 255});
    std::vector<uint8_t> data = pxa_encode(s);
    size_t at = chunk_at(data, "TMAP");
    ASSERT_TRUE(at != std::string::npos);
    /* tag, length, count (u16), name length (u8), "level" (5), then the tileset index */
    size_t index = at + 4 + 4 + 2 + 1 + 5;
    data[index] = 7;
    Sprite back = must_have(pxa_decode(data));
    ASSERT_EQ_INT(0, back.tilesets().size());
    ASSERT_EQ_INT(0, back.tilemaps().size());
    ASSERT_EQ_INT(9, back.layer(0)->cel(0)->get(1, 1).r);
}

TEST(a_truncated_tile_chunk_is_an_invalid_file)
{
    std::vector<uint8_t> data = pxa_encode(with_tiles());
    size_t at = chunk_at(data, "TMAP");
    data.resize(at + 12);
    ASSERT_TRUE(!pxa_decode(data).has_value());
}

TEST(a_huge_atlas_header_is_rejected_without_allocating)
{
    std::vector<uint8_t> data = pxa_encode(with_tiles());
    size_t at = chunk_at(data, "TSET");
    /* tag, length, count, name length, "ground" (6), tile w, tile h, atlas w */
    size_t atlasW = at + 4 + 4 + 2 + 1 + 6 + 4 + 4;
    data[atlasW + 1] = 0xff;
    data[atlasW + 2] = 0xff;
    ASSERT_TRUE(!pxa_decode(data).has_value());
}

int main()
{
    RUN(tiles_survive_a_save_and_load);
    RUN(a_maps_position_and_turned_cells_survive_a_save_and_load);
    RUN(maps_at_the_origin_write_no_position_chunk);
    RUN(a_position_out_of_range_is_dropped_and_the_tiles_stay);
    RUN(a_sprite_without_tiles_writes_no_tile_chunks);
    RUN(a_map_that_points_at_a_missing_tileset_drops_the_tiles_but_keeps_the_pixels);
    RUN(a_truncated_tile_chunk_is_an_invalid_file);
    RUN(a_huge_atlas_header_is_rejected_without_allocating);
    return TEST_SUMMARY();
}
