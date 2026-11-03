/* The floating Tilemap window: the small picture of the tile the brush paints, which follows the
 * Flip and Turn buttons. Runs under SDL_VIDEODRIVER=dummy. */
#include <memory>

#include "editor.hpp"
#include "imgui_internal.h"
#include "minitest.hpp"
#include "tilemap/tilemap_window.hpp"
#include "ui_harness.hpp"

using test_ui::Harness;

/* Lets a test read what the window drew without making that part of its interface. */
struct TilemapWindowTestAccess {
    static uint32_t brushCell(const TilemapWindow &w)
    {
        return w.shownCell_;
    }
    static int brushWidth(const TilemapWindow &w)
    {
        return w.brush_.width();
    }
    static bool brushShown(const TilemapWindow &w)
    {
        return w.brush_.texture() != nullptr;
    }
};

namespace
{

/* A 4 x 2 sprite cut into two square 2 x 2 tiles, a 2 x 1 map of them, the first tile picked. */
struct Fixture {
    Harness h;
    std::unique_ptr<Editor> editor;
    TilemapWindow window;

    Fixture()
    {
        editor = Editor::create(h.renderer, 4, 2);
        editor->activeCel()->set(0, 0, Rgba{255, 0, 0, 255});
        editor->addTilesetFromCel(2, 2);
        editor->addTilemap(0, 2, 1);
        editor->selectTilemap(0);
        editor->selectTile(1);
        editor->overlays().tilemap_window = true;
        h.body = [this] { window.show(h.renderer, *editor); };
        frames();
    }
    ~Fixture()
    {
        window.release();
    }
    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;
    Fixture(Fixture &&) = delete;
    Fixture &operator=(Fixture &&) = delete;

    void frames()
    {
        for (int i = 0; i < 3; i++) {
            h.frame(900, 650, false);
        }
    }
};

} // namespace

TEST(the_brush_tile_is_shown_as_a_picture_of_one_tile)
{
    Fixture f;
    ASSERT_TRUE(TilemapWindowTestAccess::brushShown(f.window));
    ASSERT_EQ_INT(2, TilemapWindowTestAccess::brushWidth(f.window));
    ASSERT_TRUE(TilemapWindowTestAccess::brushCell(f.window) == 1);
}

TEST(flipping_or_turning_the_tile_changes_the_picture_of_the_brush)
{
    Fixture f;
    f.editor->flipSelectedTile(true);
    f.frames();
    ASSERT_TRUE(TilemapWindowTestAccess::brushCell(f.window) == (1 | TILE_FLIP_X));
    f.editor->rotateSelectedTile(true);
    f.frames();
    ASSERT_TRUE(TilemapWindowTestAccess::brushCell(f.window) == f.editor->selectedCell());
    ASSERT_TRUE(TilemapWindowTestAccess::brushShown(f.window));
}

TEST(the_eraser_has_no_brush_picture)
{
    Fixture f;
    f.editor->selectTile(0);
    f.frames();
    ASSERT_TRUE(!TilemapWindowTestAccess::brushShown(f.window));
    ASSERT_TRUE(TilemapWindowTestAccess::brushCell(f.window) == 0);
}

TEST(picking_another_tile_updates_the_picture)
{
    Fixture f;
    f.editor->selectTile(2);
    f.frames();
    ASSERT_TRUE(TilemapWindowTestAccess::brushCell(f.window) == 2);
    ASSERT_TRUE(TilemapWindowTestAccess::brushShown(f.window));
}

TEST(a_map_of_another_tileset_gets_the_picture_of_its_own_tile)
{
    Fixture f;
    ASSERT_TRUE(f.editor->addTilesetFromCel(4, 2)); /* the whole sprite as one wide tile */
    ASSERT_TRUE(f.editor->addTilemap(1, 1, 1));
    f.editor->selectTilemap(1);
    f.editor->selectTile(1);
    f.frames();
    ASSERT_EQ_INT(4, TilemapWindowTestAccess::brushWidth(f.window));
}

/* A floating window has no collapse arrow: ImGui draws it as a filled triangle, which DESIGN.md
 * leaves out (chevrons only), and every other window of the app turns it off. */
TEST(the_tilemap_window_has_no_collapse_arrow)
{
    Fixture f;
    const ImGuiWindow *window = ImGui::FindWindowByName("Tilemap");
    ASSERT_TRUE(window != nullptr);
    ASSERT_TRUE((window->Flags & ImGuiWindowFlags_NoCollapse) != 0);
}

int main()
{
    RUN(the_brush_tile_is_shown_as_a_picture_of_one_tile);
    RUN(flipping_or_turning_the_tile_changes_the_picture_of_the_brush);
    RUN(the_eraser_has_no_brush_picture);
    RUN(picking_another_tile_updates_the_picture);
    RUN(a_map_of_another_tileset_gets_the_picture_of_its_own_tile);
    RUN(the_tilemap_window_has_no_collapse_arrow);
    return TEST_SUMMARY();
}
