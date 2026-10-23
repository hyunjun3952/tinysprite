#include "minitest.hpp"
#include "tinysprite/tilemap_ops.hpp"

static const Rgba RED = {255, 0, 0, 255};

static Tileset tiles(int count)
{
    Cel cel(2 * count, 2);
    for (int i = 0; i < count; i++) {
        cel.set(2 * i, 0, Rgba{static_cast<uint8_t>(10 * (i + 1)), 0, 0, 255});
    }
    return must_have(tileset_from_cel(cel, Rect{0, 0, 2 * count, 2}, 2, 2, "t"));
}

/* A sprite with one 3-tile tileset and one 4 x 3 map of it. */
static Sprite with_map()
{
    Sprite s(8, 8);
    s.setTileData({tiles(3)}, {tilemap_blank("map", 0, 4, 3)});
    return s;
}

TEST(adding_and_undoing_a_tileset)
{
    Sprite s(8, 8);
    History h;
    ASSERT_TRUE(tileset_op_add(h, s, tiles(2)));
    ASSERT_EQ_INT(1, s.tilesets().size());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, s.tilesets().size());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(1, s.tilesets().size());
}

TEST(an_invalid_tileset_is_refused)
{
    Sprite s(8, 8);
    History h;
    Tileset bad;
    ASSERT_TRUE(!tileset_op_add(h, s, bad));
    ASSERT_TRUE(!h.canUndo());
}

TEST(a_tileset_in_use_cannot_be_removed)
{
    Sprite s = with_map();
    History h;
    ASSERT_TRUE(!tileset_op_remove(h, s, 0));
    ASSERT_EQ_INT(1, s.tilesets().size());
}

TEST(removing_a_tileset_moves_the_maps_of_later_ones)
{
    Sprite s(8, 8);
    TileMap map = tilemap_blank("m", 1, 2, 2);
    map.cells[0] = 2;
    s.setTileData({tiles(1), tiles(3)}, {map});
    History h;
    ASSERT_TRUE(tileset_op_remove(h, s, 0));
    ASSERT_EQ_INT(1, s.tilesets().size());
    ASSERT_EQ_INT(0, s.tilemaps()[0].tileset);
    ASSERT_EQ_INT(2, s.tilemaps()[0].cell(0, 0));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(2, s.tilesets().size());
    ASSERT_EQ_INT(1, s.tilemaps()[0].tileset);
}

TEST(adding_a_map_needs_an_existing_tileset)
{
    Sprite s(8, 8);
    History h;
    ASSERT_TRUE(!tilemap_op_add(h, s, tilemap_blank("m", 0, 2, 2)));
    s.setTileData({tiles(1)}, {});
    ASSERT_TRUE(tilemap_op_add(h, s, tilemap_blank("m", 0, 2, 2)));
    ASSERT_EQ_INT(1, s.tilemaps().size());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, s.tilemaps().size());
}

TEST(removing_a_map_is_undoable)
{
    Sprite s = with_map();
    History h;
    ASSERT_TRUE(tilemap_op_remove(h, s, 0));
    ASSERT_EQ_INT(0, s.tilemaps().size());
    ASSERT_TRUE(!tilemap_op_remove(h, s, 0));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(1, s.tilemaps().size());
}

TEST(setting_a_map_renames_it_and_a_same_value_records_nothing)
{
    Sprite s = with_map();
    History h;
    TileMap same = s.tilemaps()[0];
    ASSERT_TRUE(tilemap_op_set(h, s, 0, same));
    ASSERT_TRUE(!h.canUndo());
    TileMap next = same;
    next.name = "renamed";
    next.visible = false;
    ASSERT_TRUE(tilemap_op_set(h, s, 0, next));
    ASSERT_TRUE(s.tilemaps()[0].name == "renamed");
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(s.tilemaps()[0].name == "map");
    ASSERT_TRUE(s.tilemaps()[0].visible);
}

TEST(resizing_keeps_the_cells_that_stay)
{
    Sprite s = with_map();
    s.setTile(0, 3, 2, 3);
    s.setTile(0, 0, 0, 1);
    History h;
    ASSERT_TRUE(tilemap_op_resize(h, s, 0, 2, 5));
    ASSERT_EQ_INT(2, s.tilemaps()[0].columns);
    ASSERT_EQ_INT(5, s.tilemaps()[0].rows);
    ASSERT_EQ_INT(1, s.tilemaps()[0].cell(0, 0));
    ASSERT_EQ_INT(0, s.tilemaps()[0].cell(1, 4));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(3, s.tilemaps()[0].cell(3, 2));
    ASSERT_TRUE(!tilemap_op_resize(h, s, 0, 0, 5));
    ASSERT_TRUE(!tilemap_op_resize(h, s, 0, 5, TILEMAP_MAX_DIMENSION + 1));
}

TEST(fill_sets_a_clipped_rectangle_in_one_step)
{
    Sprite s = with_map();
    History h;
    ASSERT_TRUE(tilemap_op_fill(h, s, 0, Rect{2, 1, 10, 10}, 2));
    ASSERT_EQ_INT(2, s.tilemaps()[0].cell(3, 2));
    ASSERT_EQ_INT(2, s.tilemaps()[0].cell(2, 1));
    ASSERT_EQ_INT(0, s.tilemaps()[0].cell(1, 1));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, s.tilemaps()[0].cell(3, 2));
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(!tilemap_op_fill(h, s, 0, Rect{0, 0, 1, 1}, 4));
}

TEST(a_stroke_paints_at_once_and_records_one_step)
{
    Sprite s = with_map();
    History h;
    auto stroke = TileStroke::begin(h, s, 0);
    ASSERT_TRUE(stroke != nullptr);
    ASSERT_TRUE(stroke->set(0, 0, 1));
    ASSERT_EQ_INT(1, s.tilemaps()[0].cell(0, 0));
    ASSERT_TRUE(stroke->set(1, 0, 2));
    ASSERT_TRUE(stroke->set(0, 0, 3)); /* the same cell twice */
    ASSERT_TRUE(!stroke->set(9, 0, 1));
    ASSERT_TRUE(!stroke->set(0, 0, 4));
    ASSERT_TRUE(stroke->end());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, s.tilemaps()[0].cell(0, 0));
    ASSERT_EQ_INT(0, s.tilemaps()[0].cell(1, 0));
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(3, s.tilemaps()[0].cell(0, 0));
    ASSERT_EQ_INT(2, s.tilemaps()[0].cell(1, 0));
}

TEST(a_stroke_that_changed_nothing_records_nothing)
{
    Sprite s = with_map();
    History h;
    auto stroke = TileStroke::begin(h, s, 0);
    stroke->set(0, 0, 0);
    ASSERT_TRUE(stroke->end());
    ASSERT_TRUE(!h.canUndo());
}

TEST(a_stroke_destroyed_unfinished_puts_the_cells_back)
{
    Sprite s = with_map();
    History h;
    {
        auto stroke = TileStroke::begin(h, s, 0);
        stroke->set(2, 2, 1);
    }
    ASSERT_EQ_INT(0, s.tilemaps()[0].cell(2, 2));
    ASSERT_TRUE(!h.canUndo());
}

TEST(a_stroke_needs_a_map_and_a_free_history)
{
    Sprite s = with_map();
    History h;
    ASSERT_TRUE(TileStroke::begin(h, s, 1) == nullptr);
    h.beginStroke(*s.layer(0)->cel(0));
    ASSERT_TRUE(TileStroke::begin(h, s, 0) == nullptr);
    ASSERT_TRUE(!tilemap_op_fill(h, s, 0, Rect{0, 0, 1, 1}, 1));
    h.cancelStroke();
}

TEST(tile_steps_interleave_with_pixel_steps)
{
    Sprite s = with_map();
    History h;
    tilemap_op_fill(h, s, 0, Rect{0, 0, 1, 1}, 1);
    h.beginStroke(*s.layer(0)->cel(0));
    h.setPixel(0, 0, RED);
    h.endStroke();
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(1, s.tilemaps()[0].cell(0, 0));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, s.tilemaps()[0].cell(0, 0));
}

int main()
{
    RUN(adding_and_undoing_a_tileset);
    RUN(an_invalid_tileset_is_refused);
    RUN(a_tileset_in_use_cannot_be_removed);
    RUN(removing_a_tileset_moves_the_maps_of_later_ones);
    RUN(adding_a_map_needs_an_existing_tileset);
    RUN(removing_a_map_is_undoable);
    RUN(setting_a_map_renames_it_and_a_same_value_records_nothing);
    RUN(resizing_keeps_the_cells_that_stay);
    RUN(fill_sets_a_clipped_rectangle_in_one_step);
    RUN(a_stroke_paints_at_once_and_records_one_step);
    RUN(a_stroke_that_changed_nothing_records_nothing);
    RUN(a_stroke_destroyed_unfinished_puts_the_cells_back);
    RUN(a_stroke_needs_a_map_and_a_free_history);
    RUN(tile_steps_interleave_with_pixel_steps);
    return TEST_SUMMARY();
}
