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
    return must_have(tileset_from_cel(two_tile_cel(), Rect{0, 0, 4, 2}, 2, 2, "ground"));
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
    Tileset ts = must_have(tileset_from_cel(cel, Rect{0, 0, 7, 5}, 3, 2, "t"));
    ASSERT_EQ_INT(2, ts.columns());
    ASSERT_EQ_INT(2, ts.rows());
    ASSERT_EQ_INT(6, ts.atlas->width());
    ASSERT_EQ_INT(4, ts.atlas->height());
}

TEST(a_tileset_takes_only_the_given_area)
{
    Cel cel(8, 8);
    cel.set(4, 4, BLUE);
    Tileset ts = must_have(tileset_from_cel(cel, Rect{4, 4, 4, 4}, 2, 2, "t"));
    ASSERT_TRUE(ts.atlas->get(0, 0) == BLUE);
    ASSERT_EQ_INT(4, ts.tileCount());
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
    Tileset ts = must_have(tileset_from_cel(cel, Rect{0, 0, 4, 2}, 2, 2, std::string(100, 'x')));
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
    Cel picture = must_have(tilemap_render(ts, map));
    ASSERT_EQ_INT(6, picture.width());
    ASSERT_EQ_INT(2, picture.height());
    ASSERT_TRUE(picture.get(1, 1) == RED);
    ASSERT_TRUE(picture.get(2, 0) == Rgba{});
    ASSERT_TRUE(picture.get(5, 1) == GREEN);
}

TEST(rendering_does_not_depend_on_which_tileset_slot_the_map_refers_to)
{
    /* The caller hands over the tileset the map refers to, whatever its index in the sprite. */
    Tileset ts = two_tiles();
    TileMap map = tilemap_blank("m", 3, 2, 1);
    map.cells = {2, 1};
    Cel picture = must_have(tilemap_render(ts, map));
    ASSERT_EQ_INT(4, picture.width());
    ASSERT_TRUE(picture.get(0, 0) == GREEN);
    ASSERT_TRUE(picture.get(2, 1) == RED);
}

TEST(rendering_refuses_a_map_that_does_not_fit)
{
    Tileset ts = two_tiles();
    TileMap bad = tilemap_blank("m", 0, 2, 2);
    bad.cells[0] = 9;
    ASSERT_TRUE(!tilemap_render(ts, bad).has_value());
    Cel big(8, 8);
    Tileset eight = must_have(tileset_from_cel(big, Rect{0, 0, 8, 8}, 8, 8, "t"));
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

static const Rgba WHITE = {255, 255, 255, 255};

/* One 2 x 2 tile with a different color in each corner: red top-left, green top-right, blue
 * bottom-left, white bottom-right. */
static Tileset corner_tile()
{
    Cel cel(2, 2);
    cel.set(0, 0, RED);
    cel.set(1, 0, GREEN);
    cel.set(0, 1, BLUE);
    cel.set(1, 1, WHITE);
    return must_have(tileset_from_cel(cel, Rect{0, 0, 2, 2}, 2, 2, "corners"));
}

TEST(a_cell_keeps_its_tile_number_below_the_flag_bits)
{
    uint32_t cell = 5 | TILE_FLIP_X | TILE_TRANSPOSE;
    ASSERT_EQ_INT(5, tile_number(cell));
    ASSERT_EQ_INT(5, tile_number(5));
    ASSERT_EQ_INT(0, tile_number(TILE_FLIP_Y));
}

TEST(a_flipped_tile_shows_its_pixels_mirrored)
{
    Tileset ts = corner_tile();
    ASSERT_TRUE(tile_pixel(ts, 1, 0, 0) == RED);
    ASSERT_TRUE(tile_pixel(ts, 1 | TILE_FLIP_X, 0, 0) == GREEN);
    ASSERT_TRUE(tile_pixel(ts, 1 | TILE_FLIP_Y, 0, 0) == BLUE);
    ASSERT_TRUE(tile_pixel(ts, 1 | TILE_FLIP_X | TILE_FLIP_Y, 0, 0) == WHITE);
    ASSERT_TRUE(tile_pixel(ts, 0, 0, 0) == Rgba{}); /* an empty cell */
    ASSERT_TRUE(tile_pixel(ts, 3, 0, 0) == Rgba{}); /* a tile the tileset lacks */
    ASSERT_TRUE(tile_pixel(ts, 1, 2, 0) == Rgba{}); /* outside the tile */
}

TEST(a_transposed_tile_swaps_rows_and_columns)
{
    Tileset ts = corner_tile();
    ASSERT_TRUE(tile_pixel(ts, 1 | TILE_TRANSPOSE, 1, 0) == BLUE);
    ASSERT_TRUE(tile_pixel(ts, 1 | TILE_TRANSPOSE, 0, 1) == GREEN);
    ASSERT_TRUE(tile_pixel(ts, 1 | TILE_TRANSPOSE, 0, 0) == RED);
}

TEST(rotating_a_tile_clockwise_moves_the_top_left_corner_to_the_top_right)
{
    Tileset ts = corner_tile();
    uint32_t turned = tile_rotated(1, true);
    ASSERT_TRUE(tile_pixel(ts, turned, 1, 0) == RED);
    ASSERT_TRUE(tile_pixel(ts, turned, 1, 1) == GREEN);
    ASSERT_TRUE(tile_pixel(ts, turned, 0, 0) == BLUE);
    uint32_t back = tile_rotated(1, false);
    ASSERT_TRUE(tile_pixel(ts, back, 0, 1) == RED);
    ASSERT_TRUE(tile_pixel(ts, back, 0, 0) == GREEN);
}

TEST(four_turns_or_two_flips_bring_a_tile_back_and_a_turn_undoes_its_opposite)
{
    for (uint32_t flags = 0; flags < 8; flags++) {
        uint32_t cell = 4 | (flags << 29);
        uint32_t around = cell;
        for (int i = 0; i < 4; i++) {
            around = tile_rotated(around, true);
        }
        ASSERT_EQ_INT(cell, around);
        ASSERT_EQ_INT(cell, tile_rotated(tile_rotated(cell, true), false));
        ASSERT_EQ_INT(cell, tile_flipped(tile_flipped(cell, true), true));
        ASSERT_EQ_INT(cell, tile_flipped(tile_flipped(cell, false), false));
        ASSERT_EQ_INT(4, tile_number(tile_rotated(cell, true)));
    }
}

TEST(flipping_and_turning_leave_an_empty_cell_empty)
{
    ASSERT_EQ_INT(0, tile_flipped(0, true));
    ASSERT_EQ_INT(0, tile_rotated(0, true));
}

TEST(flipping_a_turned_tile_flips_what_is_on_screen)
{
    Tileset ts = corner_tile();
    uint32_t cell = tile_flipped(tile_rotated(1, true), true); /* turn, then mirror left-right */
    ASSERT_TRUE(tile_pixel(ts, cell, 0, 0) == RED);            /* the turn put red top-right */
    ASSERT_TRUE(tile_pixel(ts, cell, 0, 1) == GREEN);
}

TEST(a_map_with_flipped_cells_is_valid_but_not_a_flag_on_nothing)
{
    std::vector<Tileset> sets = {two_tiles()};
    TileMap map = tilemap_blank("m", 0, 2, 2);
    map.cells[0] = 2 | TILE_FLIP_X | TILE_FLIP_Y | TILE_TRANSPOSE;
    ASSERT_TRUE(tilemap_valid(map, sets));
    map.cells[0] = 3 | TILE_FLIP_X; /* tile 2 does not exist */
    ASSERT_TRUE(!tilemap_valid(map, sets));
    map.cells[0] = TILE_FLIP_X; /* a flag but no tile */
    ASSERT_TRUE(!tilemap_valid(map, sets));
}

TEST(a_transposed_cell_needs_square_tiles)
{
    Cel cel(6, 2);
    Tileset wide = must_have(tileset_from_cel(cel, Rect{0, 0, 6, 2}, 3, 2, "wide"));
    TileMap map = tilemap_blank("m", 0, 1, 1);
    map.cells[0] = 1 | TILE_FLIP_X | TILE_FLIP_Y;
    ASSERT_TRUE(tilemap_valid(map, {wide}));
    map.cells[0] = 1 | TILE_TRANSPOSE;
    ASSERT_TRUE(!tilemap_valid(map, {wide}));
}

TEST(a_map_offset_must_stay_within_the_limit)
{
    std::vector<Tileset> sets = {two_tiles()};
    TileMap map = tilemap_blank("m", 0, 1, 1);
    ASSERT_EQ_INT(0, map.x);
    ASSERT_EQ_INT(0, map.y);
    map.x = -TILEMAP_OFFSET_MAX;
    map.y = TILEMAP_OFFSET_MAX;
    ASSERT_TRUE(tilemap_valid(map, sets));
    map.x = TILEMAP_OFFSET_MAX + 1;
    ASSERT_TRUE(!tilemap_valid(map, sets));
    map.x = 0;
    map.y = -TILEMAP_OFFSET_MAX - 1;
    ASSERT_TRUE(!tilemap_valid(map, sets));
}

TEST(rendering_draws_flipped_cells_flipped)
{
    Tileset ts = corner_tile();
    TileMap map = tilemap_blank("m", 0, 2, 1);
    map.cells = {1 | TILE_FLIP_X, 1};
    Cel picture = must_have(tilemap_render(ts, map));
    ASSERT_TRUE(picture.get(0, 0) == GREEN);
    ASSERT_TRUE(picture.get(2, 0) == RED);
}

TEST(set_tile_takes_a_flipped_cell_and_refuses_a_flag_on_nothing)
{
    Sprite s(4, 4);
    s.setTileData({two_tiles()}, {tilemap_blank("m", 0, 2, 2)});
    ASSERT_TRUE(s.setTile(0, 0, 0, 2 | TILE_FLIP_Y));
    ASSERT_EQ_INT(2 | TILE_FLIP_Y, s.tilemaps()[0].cell(0, 0));
    ASSERT_TRUE(!s.setTile(0, 1, 0, 3 | TILE_FLIP_Y));
    ASSERT_TRUE(!s.setTile(0, 1, 0, TILE_FLIP_Y));
}

int main()
{
    RUN(a_cell_keeps_its_tile_number_below_the_flag_bits);
    RUN(a_flipped_tile_shows_its_pixels_mirrored);
    RUN(a_transposed_tile_swaps_rows_and_columns);
    RUN(rotating_a_tile_clockwise_moves_the_top_left_corner_to_the_top_right);
    RUN(four_turns_or_two_flips_bring_a_tile_back_and_a_turn_undoes_its_opposite);
    RUN(flipping_and_turning_leave_an_empty_cell_empty);
    RUN(flipping_a_turned_tile_flips_what_is_on_screen);
    RUN(a_map_with_flipped_cells_is_valid_but_not_a_flag_on_nothing);
    RUN(a_transposed_cell_needs_square_tiles);
    RUN(a_map_offset_must_stay_within_the_limit);
    RUN(rendering_draws_flipped_cells_flipped);
    RUN(set_tile_takes_a_flipped_cell_and_refuses_a_flag_on_nothing);
    RUN(a_tileset_is_cut_from_a_cel_row_by_row);
    RUN(a_tileset_leaves_out_partial_tiles_at_the_edges);
    RUN(a_tileset_takes_only_the_given_area);
    RUN(a_tileset_is_refused_for_a_bad_size_or_area);
    RUN(a_tileset_name_is_cut_and_the_atlas_is_independent_of_the_cel);
    RUN(a_blank_map_has_empty_cells_and_checks_its_size);
    RUN(a_map_is_valid_only_for_tiles_that_exist);
    RUN(rendering_draws_each_cell_with_its_tile);
    RUN(rendering_does_not_depend_on_which_tileset_slot_the_map_refers_to);
    RUN(rendering_refuses_a_map_that_does_not_fit);
    RUN(a_sprite_keeps_tilesets_and_maps_that_fit_together);
    RUN(a_sprite_refuses_tile_data_that_does_not_fit_and_keeps_what_it_had);
    RUN(set_tile_changes_one_cell_if_the_tile_exists);
    return TEST_SUMMARY();
}
