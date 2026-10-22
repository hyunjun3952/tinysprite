/* The canvas with synthetic mouse input: the right and middle buttons pan, a long middle press
 * picks a color, the left one still draws. Runs under SDL_VIDEODRIVER=dummy. */
#include <memory>

#include "canvas.hpp"
#include "editor.hpp"
#include "minitest.hpp"
#include "ui_harness.hpp"

using test_ui::Harness;

namespace
{

struct Fixture {
    Harness h;
    std::unique_ptr<Editor> editor;
    std::unique_ptr<Canvas> canvas;

    Fixture()
    {
        editor = Editor::create(h.renderer, 16, 16);
        canvas = std::make_unique<Canvas>(*editor);
        h.body = [this] { canvas->show(250.0f, 0.0f, 490.0f, 600.0f); };
        for (int i = 0; i < 4; i++) {
            h.frame(400, 300, false);
        }
    }

    /* Drags with `button` held from (x0, y0) to (x1, y1), in steps. */
    void drag(int button, float x0, float y0, float x1, float y1)
    {
        h.button = button;
        h.frame(x0, y0, false);
        h.frame(x0, y0, true);
        for (int i = 1; i <= 10; i++) {
            float t = static_cast<float>(i) / 10.0f;
            h.frame(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, true);
        }
        h.frame(x1, y1, false);
        h.frame(x1, y1, false);
        h.button = 0;
    }
};

} // namespace

TEST(right_drag_pans_the_view)
{
    Fixture f;
    float x = f.editor->view().viewport.pan_x;
    float y = f.editor->view().viewport.pan_y;
    f.drag(1, 400, 300, 460, 340);
    ASSERT_TRUE(f.editor->view().viewport.pan_x - x > 55.0f);
    ASSERT_TRUE(f.editor->view().viewport.pan_x - x < 65.0f);
    ASSERT_TRUE(f.editor->view().viewport.pan_y - y > 35.0f);
    ASSERT_TRUE(f.editor->view().viewport.pan_y - y < 45.0f);
}

TEST(right_drag_does_not_draw)
{
    Fixture f;
    f.drag(1, 400, 300, 460, 340);
    bool drawn = false;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            drawn = drawn || f.editor->sprite().layer(0)->cel(0)->get(x, y).a != 0;
        }
    }
    ASSERT_TRUE(!drawn);
    ASSERT_TRUE(!f.editor->history().canUndo());
}

TEST(left_drag_still_draws_and_does_not_pan)
{
    Fixture f;
    float x = f.editor->view().viewport.pan_x;
    f.drag(0, 400, 300, 460, 340);
    ASSERT_TRUE(f.editor->view().viewport.pan_x == x);
    ASSERT_TRUE(f.editor->history().canUndo());
}

TEST(a_right_click_without_moving_leaves_the_view_alone)
{
    Fixture f;
    float x = f.editor->view().viewport.pan_x;
    float y = f.editor->view().viewport.pan_y;
    f.drag(1, 400, 300, 400, 300);
    ASSERT_TRUE(f.editor->view().viewport.pan_x == x);
    ASSERT_TRUE(f.editor->view().viewport.pan_y == y);
}

TEST(the_wheel_zooms_at_the_cursor)
{
    Fixture f;
    float zoom = f.editor->view().viewport.zoom;
    ImGui::GetIO().AddMouseWheelEvent(0.0f, 1.0f);
    f.h.frame(400, 300, false);
    ASSERT_TRUE(f.editor->view().viewport.zoom > zoom); /* up zooms in */
    float zoomedIn = f.editor->view().viewport.zoom;
    ImGui::GetIO().AddMouseWheelEvent(0.0f, -1.0f);
    ImGui::GetIO().AddMouseWheelEvent(0.0f, -1.0f);
    f.h.frame(400, 300, false);
    f.h.frame(400, 300, false);
    ASSERT_TRUE(f.editor->view().viewport.zoom < zoomedIn); /* down zooms out */
    ASSERT_TRUE(!f.editor->view().auto_fit);
}

/* Holds the middle button still at (x, y) for `seconds` of real time (the hold is timed by the
 * clock, not by frames). */
static void hold_middle(Fixture &f, float x, float y, double seconds)
{
    f.h.button = 2;
    f.h.frame(x, y, false);
    Uint32 start = SDL_GetTicks();
    do {
        f.h.frame(x, y, true);
        SDL_Delay(5);
    } while (SDL_GetTicks() - start < static_cast<Uint32>(seconds * 1000.0));
}

TEST(middle_drag_pans_the_view)
{
    Fixture f;
    float x = f.editor->view().viewport.pan_x;
    f.drag(2, 400, 300, 460, 340);
    ASSERT_TRUE(f.editor->view().viewport.pan_x - x > 55.0f);
    ASSERT_TRUE(f.editor->view().viewport.pan_x - x < 65.0f);
    ASSERT_TRUE(!f.editor->history().canUndo());
}

TEST(a_long_middle_press_picks_the_color_under_the_cursor)
{
    Fixture f;
    Rgba teal = {0, 128, 128, 255};
    int mx = f.editor->view().viewport.screenToCelX(400.0f - 250.0f);
    int my = f.editor->view().viewport.screenToCelY(300.0f);
    f.editor->sprite().layer(0)->cel(0)->set(mx, my, teal);
    f.editor->markAllDirty();
    hold_middle(f, 400, 300, 0.6);
    ASSERT_TRUE(f.editor->foreground() == teal);
    ASSERT_TRUE(f.editor->tool() == ToolKind::Eyedropper);
    f.h.frame(400, 300, false);
    f.h.frame(400, 300, false);
    ASSERT_TRUE(f.editor->tool() == ToolKind::Pencil); /* back to the tool it replaced */
    f.h.button = 0;
}

TEST(a_short_middle_click_neither_picks_nor_pans)
{
    Fixture f;
    float x = f.editor->view().viewport.pan_x;
    Rgba before = f.editor->foreground();
    f.drag(2, 400, 300, 400, 300);
    ASSERT_TRUE(f.editor->view().viewport.pan_x == x);
    ASSERT_TRUE(f.editor->foreground() == before);
}

int main()
{
    RUN(the_wheel_zooms_at_the_cursor);
    RUN(right_drag_pans_the_view);
    RUN(right_drag_does_not_draw);
    RUN(left_drag_still_draws_and_does_not_pan);
    RUN(a_right_click_without_moving_leaves_the_view_alone);
    RUN(middle_drag_pans_the_view);
    RUN(a_long_middle_press_picks_the_color_under_the_cursor);
    RUN(a_short_middle_click_neither_picks_nor_pans);
    return TEST_SUMMARY();
}
