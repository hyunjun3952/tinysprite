/* Headless tests for the tilemap side of the UI editor: making tilesets from the active cel and
 * tilemaps from them, the selected map and tile, painting with a stroke and undo. Built only when
 * the UI is built; runs under SDL_VIDEODRIVER=dummy. */

#include "editor.hpp"
#include "minitest.hpp"
#include "ui_editor_support.hpp"

/* An 8 x 6 editor whose cel has a red pixel at the top-left of each 4 x 3 tile. */
static Editor *editor_with_tiles()
{
    Editor *e = make_editor();
    for (int ty = 0; ty < 2; ty++) {
        for (int tx = 0; tx < 2; tx++) {
            e->activeCel()->set(tx * 4, ty * 3,
                                Rgba{static_cast<uint8_t>(50 * (tx + 2 * ty + 1)), 0, 0, 255});
        }
    }
    return e;
}

TEST(a_tileset_is_made_from_the_whole_cel_and_selected)
{
    Editor *e = editor_with_tiles();
    ASSERT_TRUE(e->addTilesetFromCel(4, 3));
    ASSERT_EQ_INT(1, e->sprite().tilesets().size());
    ASSERT_EQ_INT(4, e->sprite().tilesets()[0].tileCount());
    ASSERT_TRUE(e->sprite().tilesets()[0].name == "Tileset 1");
    ASSERT_EQ_INT(0, e->selectedTileset());
    ASSERT_TRUE(e->addTilesetFromCel(2, 3));
    ASSERT_TRUE(e->sprite().tilesets()[1].name == "Tileset 2");
    e->undo();
    ASSERT_EQ_INT(1, e->sprite().tilesets().size());
    delete e;
}

TEST(a_tileset_is_made_from_the_selection_when_there_is_one)
{
    Editor *e = editor_with_tiles();
    e->setSelection(Rect{4, 0, 4, 3});
    ASSERT_TRUE(e->addTilesetFromCel(4, 3));
    ASSERT_EQ_INT(1, e->sprite().tilesets()[0].tileCount());
    ASSERT_EQ_INT(100, e->sprite().tilesets()[0].atlas->get(0, 0).r);
    delete e;
}

TEST(a_tile_size_that_does_not_fit_is_refused)
{
    Editor *e = editor_with_tiles();
    ASSERT_TRUE(!e->addTilesetFromCel(9, 3));
    ASSERT_TRUE(!e->addTilesetFromCel(0, 3));
    ASSERT_EQ_INT(0, e->sprite().tilesets().size());
    delete e;
}

TEST(a_tilemap_is_made_from_a_tileset_and_selected)
{
    Editor *e = editor_with_tiles();
    ASSERT_TRUE(!e->addTilemap(0, 4, 4));
    e->addTilesetFromCel(4, 3);
    ASSERT_TRUE(e->addTilemap(0, 5, 4));
    ASSERT_EQ_INT(1, e->sprite().tilemaps().size());
    ASSERT_TRUE(e->sprite().tilemaps()[0].name == "Map 1");
    ASSERT_EQ_INT(0, e->selectedTilemap());
    ASSERT_TRUE(!e->addTilemap(0, 0, 4));
    ASSERT_TRUE(!e->addTilemap(3, 4, 4));
    delete e;
}

TEST(painting_with_a_stroke_is_one_undo_step)
{
    Editor *e = editor_with_tiles();
    e->addTilesetFromCel(4, 3);
    e->addTilemap(0, 4, 4);
    e->selectTile(3);
    auto stroke = e->beginTileStroke();
    ASSERT_TRUE(stroke != nullptr);
    stroke->set(0, 0, e->selectedTile());
    stroke->set(1, 0, e->selectedTile());
    ASSERT_TRUE(stroke->end());
    ASSERT_EQ_INT(3, e->sprite().tilemaps()[0].cell(1, 0));
    e->undo();
    ASSERT_EQ_INT(0, e->sprite().tilemaps()[0].cell(1, 0));
    ASSERT_EQ_INT(1, e->sprite().tilemaps().size());
    delete e;
}

TEST(the_selected_tile_stays_within_the_tileset)
{
    Editor *e = editor_with_tiles();
    e->addTilesetFromCel(4, 3);
    e->addTilemap(0, 4, 4);
    e->selectTile(99);
    ASSERT_EQ_INT(4, e->selectedTile());
    e->selectTile(0);
    ASSERT_EQ_INT(0, e->selectedTile());
    delete e;
}

TEST(removing_and_resizing_maps_and_tilesets)
{
    Editor *e = editor_with_tiles();
    e->addTilesetFromCel(4, 3);
    e->addTilemap(0, 4, 4);
    ASSERT_TRUE(!e->removeTileset(0)); /* in use */
    ASSERT_TRUE(e->resizeTilemap(0, 6, 2));
    ASSERT_EQ_INT(6, e->sprite().tilemaps()[0].columns);
    ASSERT_TRUE(e->setTilemapVisible(0, false));
    ASSERT_TRUE(!e->sprite().tilemaps()[0].visible);
    ASSERT_TRUE(e->renameTilemap(0, "Ground"));
    ASSERT_TRUE(e->sprite().tilemaps()[0].name == "Ground");
    ASSERT_TRUE(e->removeTilemap(0));
    ASSERT_EQ_INT(-1, e->selectedTilemap());
    ASSERT_TRUE(e->removeTileset(0));
    ASSERT_EQ_INT(-1, e->selectedTileset());
    delete e;
}

TEST(removing_a_tileset_below_the_selected_one_keeps_the_selection_on_it)
{
    Editor *e = make_editor();
    ASSERT_TRUE(e->addTilesetFromCel(2, 2));
    ASSERT_TRUE(e->addTilesetFromCel(2, 2));
    ASSERT_TRUE(e->addTilesetFromCel(2, 2));
    e->selectTileset(1);
    std::string name = e->sprite().tilesets()[1].name;
    ASSERT_TRUE(e->removeTileset(0));
    ASSERT_EQ_INT(0, e->selectedTileset());
    ASSERT_TRUE(e->sprite().tilesets()[0].name == name);
    delete e;
}

TEST(removing_a_tilemap_below_the_selected_one_keeps_the_selection_on_it)
{
    Editor *e = make_editor();
    ASSERT_TRUE(e->addTilesetFromCel(2, 2));
    ASSERT_TRUE(e->addTilemap(0, 2, 2));
    ASSERT_TRUE(e->addTilemap(0, 2, 2));
    ASSERT_TRUE(e->addTilemap(0, 2, 2));
    e->selectTilemap(1);
    std::string name = e->sprite().tilemaps()[1].name;
    ASSERT_TRUE(e->removeTilemap(0));
    ASSERT_EQ_INT(0, e->selectedTilemap());
    ASSERT_TRUE(e->sprite().tilemaps()[0].name == name);
    delete e;
}

TEST(the_revision_changes_with_every_tile_edit_and_undo)
{
    Editor *e = editor_with_tiles();
    uint64_t r0 = e->tilesRevision();
    e->addTilesetFromCel(4, 3);
    uint64_t r1 = e->tilesRevision();
    ASSERT_TRUE(r1 != r0);
    e->undo();
    ASSERT_TRUE(e->tilesRevision() != r1);
    delete e;
}

TEST(tile_edits_are_held_off_while_a_tool_is_dragged)
{
    Editor *e = editor_with_tiles();
    e->addTilesetFromCel(4, 3);
    e->addTilemap(0, 4, 4);
    e->press(1, 1);
    ASSERT_TRUE(e->stroking());
    ASSERT_TRUE(!e->addTilemap(0, 2, 2));
    ASSERT_TRUE(!e->resizeTilemap(0, 2, 2));
    ASSERT_TRUE(e->beginTileStroke() == nullptr);
    e->release();
    delete e;
}

TEST(undo_and_redo_are_held_off_while_tiles_are_painted)
{
    Editor *e = editor_with_tiles();
    ASSERT_TRUE(e->addTilesetFromCel(4, 3));
    ASSERT_TRUE(e->addTilemap(0, 2, 2));
    ASSERT_TRUE(e->paintTile(0, 0, 1));
    ASSERT_TRUE(e->endTilePaint());
    ASSERT_TRUE(e->paintTile(0, 0, 2));
    ASSERT_TRUE(e->stroking());
    e->undo(); /* Ctrl+Z with the mouse still held: nothing happens */
    ASSERT_EQ_INT(1, e->sprite().tilemaps().size());
    ASSERT_TRUE(e->endTilePaint());
    ASSERT_TRUE(!e->stroking());
    e->undo();
    ASSERT_EQ_INT(1, e->sprite().tilemaps()[0].cell(0, 0));
    e->undo();
    ASSERT_EQ_INT(0, e->sprite().tilemaps()[0].cell(0, 0));
    delete e;
}

TEST(cancelling_puts_the_painted_tiles_back)
{
    Editor *e = editor_with_tiles();
    ASSERT_TRUE(e->addTilesetFromCel(4, 3));
    ASSERT_TRUE(e->addTilemap(0, 2, 2));
    ASSERT_TRUE(e->paintTile(1, 1, 2));
    e->cancelDrag();
    ASSERT_TRUE(!e->tilePainting());
    ASSERT_TRUE(!e->stroking());
    ASSERT_EQ_INT(0, e->sprite().tilemaps()[0].cell(1, 1));
    delete e;
}

TEST(the_canvas_shows_a_visible_tilemap_and_follows_its_edits)
{
    Editor *e = editor_with_tiles();
    e->addTilesetFromCel(4, 3);
    e->addTilemap(0, 2, 2);
    e->updateTexture();
    ASSERT_EQ_INT(200, e->shownPixel(4, 3).r); /* the layer's own pixel */
    e->paintTile(1, 1, 1);                     /* tile 0, red 50 at its top-left */
    e->endTilePaint();
    e->updateTexture();
    ASSERT_EQ_INT(50, e->shownPixel(4, 3).r);
    e->setTilemapVisible(0, false);
    e->updateTexture();
    ASSERT_EQ_INT(200, e->shownPixel(4, 3).r);
    e->undo();
    e->updateTexture();
    ASSERT_EQ_INT(50, e->shownPixel(4, 3).r);
    e->undo();
    e->updateTexture();
    ASSERT_EQ_INT(200, e->shownPixel(4, 3).r);
    delete e;
}

int main()
{
    if (!ui_init()) {
        return 1;
    }
    RUN(a_tileset_is_made_from_the_whole_cel_and_selected);
    RUN(a_tileset_is_made_from_the_selection_when_there_is_one);
    RUN(a_tile_size_that_does_not_fit_is_refused);
    RUN(a_tilemap_is_made_from_a_tileset_and_selected);
    RUN(painting_with_a_stroke_is_one_undo_step);
    RUN(the_selected_tile_stays_within_the_tileset);
    RUN(removing_and_resizing_maps_and_tilesets);
    RUN(removing_a_tileset_below_the_selected_one_keeps_the_selection_on_it);
    RUN(removing_a_tilemap_below_the_selected_one_keeps_the_selection_on_it);
    RUN(the_revision_changes_with_every_tile_edit_and_undo);
    RUN(tile_edits_are_held_off_while_a_tool_is_dragged);
    RUN(undo_and_redo_are_held_off_while_tiles_are_painted);
    RUN(cancelling_puts_the_painted_tiles_back);
    RUN(the_canvas_shows_a_visible_tilemap_and_follows_its_edits);
    ui_quit();
    return TEST_SUMMARY();
}
