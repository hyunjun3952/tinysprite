#include <stdexcept>

#include "minitest.hpp"
#include "tinysprite/sprite.hpp"
#include "tinysprite/tilemap.hpp"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba GREEN = {0, 255, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

/* A 4 x 2 cel of 2 x 2 tiles: tile 0 red, tile 1 green. */
static Cel two_tile_cel()
{
    Cel cel(4, 2);
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 2; x++) {
            cel.set(x, y, RED);
            cel.set(x + 2, y, GREEN);
        }
    }
    return cel;
}

static Tileset two_tiles()
{
    return *tileset_from_cel(two_tile_cel(), Rect{0, 0, 4, 2}, 2, 2, "ground");
}

TEST(a_tileset_is_cut_from_a_cel_row_by_row)
{
    Tileset ts = two_tiles();
    ASSERT_EQ_INT(2, ts.columns());
    ASSERT_EQ_INT(1, ts.rows());
    ASSERT_EQ_INT(2, ts.tileCount());
    ASSERT_TRUE(tileset_valid(ts));
    ASSERT_TRUE(ts.atlas->get(0, 0) == RED);
    ASSERT_TRUE(ts.atlas->get(2, 0) == GREEN);
}

TEST(a_tileset_leaves_out_partial_tiles_at_the_edges)
{
    Cel cel(7, 5);
    cel.set(6, 4, RED);
    std::optional<Tileset> ts = tileset_from_cel(cel, Rect{0, 0, 7, 5}, 3, 2, "t");
    ASSERT_TRUE(ts.has_value());
    ASSERT_EQ_INT(2, ts->columns());
    ASSERT_EQ_INT(2, ts->rows());
    ASSERT_EQ_INT(6, ts->atlas->width());
    ASSERT_EQ_INT(4, ts->atlas->height());
}

TEST(a_tileset_takes_only_the_given_area)
{
    Cel cel(8, 8);
    cel.set(4, 4, BLUE);
    std::optional<Tileset> ts = tileset_from_cel(cel, Rect{4, 4, 4, 4}, 2, 2, "t");
    ASSERT_TRUE(ts.has_value());
    ASSERT_TRUE(ts->atlas->get(0, 0) == BLUE);
    ASSERT_EQ_INT(4, ts->tileCount());
}

TEST(a_tileset_is_refused_for_a_bad_size_or_area)
{
    Cel cel(8, 8);
    ASSERT_TRUE(!tileset_from_cel(cel, Rect{0, 0, 8, 8}, 0, 2, "t"));
    ASSERT_TRUE(!tileset_from_cel(cel, Rect{0, 0, 8, 8}, 2, TILE_SIZE_MAX + 1, "t"));
    ASSERT_TRUE(!tileset_from_cel(cel, Rect{0, 0, 3, 8}, 4, 4, "t"));
    ASSERT_TRUE(!tileset_from_cel(cel, Rect{20, 20, 4, 4}, 2, 2, "t"));
}

TEST(a_tileset_name_is_cut_and_the_atlas_is_independent_of_the_cel)
{
    Cel cel = two_tile_cel();
    Tileset ts = *tileset_from_cel(cel, Rect{0, 0, 4, 2}, 2, 2, std::string(100, 'x'));
    ASSERT_EQ_INT(TILE_NAME_MAX, ts.name.size());
    cel.set(0, 0, BLUE);
    ASSERT_TRUE(ts.atlas->get(0, 0) == RED);
}

TEST(a_blank_map_has_empty_cells_and_checks_its_size)
{
    TileMap map = tilemap_blank("m", 0, 3, 2);
    ASSERT_EQ_INT(6, map.cells.size());
    ASSERT_EQ_INT(0, map.cell(2, 1));
    ASSERT_EQ_INT(0, map.cell(9, 9));
    ASSERT_THROWS(tilemap_blank("m", 0, 0, 2), std::invalid_argument);
    ASSERT_THROWS(tilemap_blank("m", 0, 2, TILEMAP_MAX_DIMENSION + 1), std::invalid_argument);
}

TEST(a_map_is_valid_only_for_tiles_that_exist)
{
    std::vector<Tileset> sets = {two_tiles()};
    TileMap map = tilemap_blank("m", 0, 2, 2);
    ASSERT_TRUE(tilemap_valid(map, sets));
    map.cells[0] = 2; /* tile 1 */
    ASSERT_TRUE(tilemap_valid(map, sets));
    map.cells[0] = 3; /* tile 2 does not exist */
    ASSERT_TRUE(!tilemap_valid(map, sets));
    map.cells[0] = 0;
    map.tileset = 1;
    ASSERT_TRUE(!tilemap_valid(map, sets));
    map.tileset = 0;
    map.cells.pop_back();
    ASSERT_TRUE(!tilemap_valid(map, sets));
}

TEST(rendering_draws_each_cell_with_its_tile)
{
    Tileset ts = two_tiles();
    TileMap map = tilemap_blank("m", 0, 3, 1);
    map.cells = {1, 0, 2};
    std::optional<Cel> picture = tilemap_render(ts, map);
    ASSERT_TRUE(picture.has_value());
    ASSERT_EQ_INT(6, picture->width());
    ASSERT_EQ_INT(2, picture->height());
    ASSERT_TRUE(picture->get(1, 1) == RED);
    ASSERT_TRUE(picture->get(2, 0) == Rgba{});
    ASSERT_TRUE(picture->get(5, 1) == GREEN);
}

TEST(rendering_refuses_a_map_that_does_not_fit)
{
    Tileset ts = two_tiles();
    TileMap bad = tilemap_blank("m", 0, 2, 2);
    bad.cells[0] = 9;
    ASSERT_TRUE(!tilemap_render(ts, bad).has_value());
    Cel big(8, 8);
    Tileset eight = *tileset_from_cel(big, Rect{0, 0, 8, 8}, 8, 8, "t");
    TileMap wide = tilemap_blank("m", 0, 1024, 1); /* 8192 px */
    ASSERT_TRUE(!tilemap_render(eight, wide).has_value());
}

TEST(a_sprite_keeps_tilesets_and_maps_that_fit_together)
{
    Sprite s(4, 4);
    TileMap map = tilemap_blank("m", 0, 2, 2);
    map.cells[3] = 2;
    ASSERT_TRUE(s.setTileData({two_tiles()}, {map}));
    ASSERT_EQ_INT(1, s.tilesets().size());
    ASSERT_EQ_INT(2, s.tilemaps()[0].cell(1, 1));
}

TEST(a_sprite_refuses_tile_data_that_does_not_fit_and_keeps_what_it_had)
{
    Sprite s(4, 4);
    s.setTileData({two_tiles()}, {tilemap_blank("m", 0, 2, 2)});
    TileMap bad = tilemap_blank("m", 3, 2, 2);
    ASSERT_TRUE(!s.setTileData({two_tiles()}, {bad}));
    ASSERT_EQ_INT(1, s.tilemaps().size());
    ASSERT_EQ_INT(0, s.tilemaps()[0].tileset);
    Tileset broken = two_tiles();
    broken.tileWidth = 3; /* the atlas is not a whole number of 3 px tiles */
    ASSERT_TRUE(!s.setTileData({broken}, {}));
}

TEST(set_tile_changes_one_cell_if_the_tile_exists)
{
    Sprite s(4, 4);
    s.setTileData({two_tiles()}, {tilemap_blank("m", 0, 2, 2)});
    ASSERT_TRUE(s.setTile(0, 1, 0, 2));
    ASSERT_EQ_INT(2, s.tilemaps()[0].cell(1, 0));
    ASSERT_TRUE(!s.setTile(0, 1, 0, 3));
    ASSERT_TRUE(!s.setTile(0, 2, 0, 1));
    ASSERT_TRUE(!s.setTile(1, 0, 0, 1));
    ASSERT_EQ_INT(2, s.tilemaps()[0].cell(1, 0));
}

int main()
{
    RUN(a_tileset_is_cut_from_a_cel_row_by_row);
    RUN(a_tileset_leaves_out_partial_tiles_at_the_edges);
    RUN(a_tileset_takes_only_the_given_area);
    RUN(a_tileset_is_refused_for_a_bad_size_or_area);
    RUN(a_tileset_name_is_cut_and_the_atlas_is_independent_of_the_cel);
    RUN(a_blank_map_has_empty_cells_and_checks_its_size);
    RUN(a_map_is_valid_only_for_tiles_that_exist);
    RUN(rendering_draws_each_cell_with_its_tile);
    RUN(rendering_refuses_a_map_that_does_not_fit);
    RUN(a_sprite_keeps_tilesets_and_maps_that_fit_together);
    RUN(a_sprite_refuses_tile_data_that_does_not_fit_and_keeps_what_it_had);
    RUN(set_tile_changes_one_cell_if_the_tile_exists);
    return TEST_SUMMARY();
}
