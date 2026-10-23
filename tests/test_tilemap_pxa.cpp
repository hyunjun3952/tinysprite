#include "minitest.hpp"
#include "tinysprite/io.hpp"

static Sprite with_tiles()
{
    Sprite s(8, 8);
    Cel cel(4, 2);
    cel.set(0, 0, Rgba{255, 0, 0, 255});
    cel.set(3, 1, Rgba{0, 0, 255, 128});
    std::vector<Tileset> sets = {*tileset_from_cel(cel, Rect{0, 0, 4, 2}, 2, 2, "ground")};
    TileMap map = tilemap_blank("level", 0, 3, 2);
    map.cells = {1, 0, 2, 2, 1, 0};
    map.visible = false;
    s.setTileData(sets, {map});
    return s;
}

TEST(tiles_survive_a_save_and_load)
{
    std::optional<Sprite> back = pxa_decode(pxa_encode(with_tiles()));
    ASSERT_TRUE(back.has_value());
    ASSERT_EQ_INT(1, back->tilesets().size());
    const Tileset &ts = back->tilesets()[0];
    ASSERT_TRUE(ts.name == "ground");
    ASSERT_EQ_INT(2, ts.tileWidth);
    ASSERT_EQ_INT(2, ts.tileCount());
    ASSERT_EQ_INT(255, ts.atlas->get(0, 0).r);
    ASSERT_EQ_INT(128, ts.atlas->get(3, 1).a);
    ASSERT_EQ_INT(1, back->tilemaps().size());
    const TileMap &map = back->tilemaps()[0];
    ASSERT_TRUE(map.name == "level");
    ASSERT_TRUE(!map.visible);
    ASSERT_EQ_INT(3, map.columns);
    ASSERT_EQ_INT(2, map.cell(2, 0));
    ASSERT_EQ_INT(1, map.cell(1, 1));
}

TEST(a_sprite_without_tiles_writes_no_tile_chunks)
{
    Sprite s(4, 4);
    std::vector<uint8_t> data = pxa_encode(s);
    std::string text(data.begin(), data.end());
    ASSERT_TRUE(text.find("TSET") == std::string::npos);
    ASSERT_TRUE(text.find("TMAP") == std::string::npos);
}

/* Replaces the first 4 bytes after `tag` ... of the chunk named `tag` in an encoded file. */
static size_t chunk_at(const std::vector<uint8_t> &data, const char *tag)
{
    std::string text(data.begin(), data.end());
    return text.find(tag);
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
    std::optional<Sprite> back = pxa_decode(data);
    ASSERT_TRUE(back.has_value());
    ASSERT_EQ_INT(0, back->tilesets().size());
    ASSERT_EQ_INT(0, back->tilemaps().size());
    ASSERT_EQ_INT(9, back->layer(0)->cel(0)->get(1, 1).r);
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
    RUN(a_sprite_without_tiles_writes_no_tile_chunks);
    RUN(a_map_that_points_at_a_missing_tileset_drops_the_tiles_but_keeps_the_pixels);
    RUN(a_truncated_tile_chunk_is_an_invalid_file);
    RUN(a_huge_atlas_header_is_rejected_without_allocating);
    return TEST_SUMMARY();
}
