/* The rulers and the guides: the tick spacing, picking a guide, and the canvas creating, moving and
 * deleting guides with the mouse. Runs under SDL_VIDEODRIVER=dummy. */
#include <cmath>
#include <memory>

#include "canvas.hpp"
#include "editor.hpp"
#include "minitest.hpp"
#include "ruler.hpp"
#include "theme.hpp"
#include "ui_harness.hpp"

using test_ui::Harness;

namespace
{

constexpr float CANVAS_X = 250.0f; /* where the fixture places the canvas window */

struct Fixture {
    Harness h;
    std::unique_ptr<Editor> editor;
    std::unique_ptr<Canvas> canvas;
    float ruler = theme::px(RULER_THICKNESS);

    explicit Fixture(bool rulers = true)
    {
        editor = Editor::create(h.renderer, 16, 16);
        editor->overlays().rulers = rulers;
        canvas = std::make_unique<Canvas>(*editor);
        h.body = [this] { canvas->show(CANVAS_X, 0.0f, 490.0f, 600.0f); };
        for (int i = 0; i < 4; i++) {
            h.frame(400, 300, false);
        }
    }

    const Viewport &view() const
    {
        return editor->view().viewport;
    }

    /* The cel edge a guide dropped at screen `y` lands on. */
    int edgeAtY(float y) const
    {
        return static_cast<int>(std::floor((y - ruler - view().pan_y) / view().zoom + 0.5f));
    }
    int edgeAtX(float x) const
    {
        return static_cast<int>(
            std::floor((x - CANVAS_X - ruler - view().pan_x) / view().zoom + 0.5f));
    }
};

} // namespace

TEST(ticks_are_a_round_number_of_cels_and_stay_apart_on_screen)
{
    ASSERT_EQ_INT(1, ruler_step(64.0f));
    ASSERT_EQ_INT(5, ruler_step(16.0f));
    ASSERT_EQ_INT(20, ruler_step(4.0f));
    ASSERT_EQ_INT(100, ruler_step(1.0f));
    ASSERT_EQ_INT(1000, ruler_step(0.0625f));
}

TEST(a_guide_snaps_to_the_nearest_cel_edge)
{
    ASSERT_EQ_INT(0, guide_snap(10.0f, 8.0f, 10.0f));
    ASSERT_EQ_INT(0, guide_snap(10.0f, 8.0f, 13.0f));
    ASSERT_EQ_INT(1, guide_snap(10.0f, 8.0f, 15.0f));
    ASSERT_EQ_INT(-1, guide_snap(10.0f, 8.0f, 2.0f));
}

TEST(a_guide_is_picked_only_within_reach_and_the_nearest_wins)
{
    const std::vector<int> lines = {2, 3, 10};
    ASSERT_EQ_INT(0, guide_pick(lines, 0.0f, 8.0f, 17.0f));  /* 2 * 8 = 16 */
    ASSERT_EQ_INT(1, guide_pick(lines, 0.0f, 8.0f, 23.0f));  /* 3 * 8 = 24 */
    ASSERT_EQ_INT(-1, guide_pick(lines, 0.0f, 8.0f, 40.0f)); /* nothing near */
    ASSERT_EQ_INT(-1, guide_pick({}, 0.0f, 8.0f, 0.0f));
}

TEST(dragging_from_the_top_ruler_makes_a_horizontal_guide)
{
    Fixture f;
    f.h.drag(500, f.ruler / 2.0f, 500, 300);
    ASSERT_EQ_INT(1, static_cast<int>(f.editor->view().guides.horizontal.size()));
    ASSERT_EQ_INT(f.edgeAtY(300), f.editor->view().guides.horizontal[0]);
    ASSERT_TRUE(f.editor->view().guides.vertical.empty());
    ASSERT_TRUE(!f.editor->history().canUndo()); /* the drag drew nothing */
}

TEST(dragging_from_the_left_ruler_makes_a_vertical_guide)
{
    Fixture f;
    f.h.drag(CANVAS_X + f.ruler / 2.0f, 300, 500, 300);
    ASSERT_EQ_INT(1, static_cast<int>(f.editor->view().guides.vertical.size()));
    ASSERT_EQ_INT(f.edgeAtX(500), f.editor->view().guides.vertical[0]);
    ASSERT_TRUE(f.editor->view().guides.horizontal.empty());
}

TEST(a_guide_dragged_back_onto_the_ruler_is_deleted)
{
    Fixture f;
    f.h.drag(500, f.ruler / 2.0f, 500, 300);
    const float y = f.ruler + f.view().pan_y +
                    static_cast<float>(f.editor->view().guides.horizontal[0]) * f.view().zoom;
    f.h.drag(500, y, 500, f.ruler / 2.0f);
    ASSERT_TRUE(f.editor->view().guides.empty());
}

TEST(an_existing_guide_is_moved_by_dragging_it)
{
    Fixture f;
    f.editor->view().guides.horizontal.push_back(4);
    const float y = f.ruler + f.view().pan_y + 4.0f * f.view().zoom;
    f.h.drag(500, y, 500, y + 3.0f * f.view().zoom);
    ASSERT_EQ_INT(1, static_cast<int>(f.editor->view().guides.horizontal.size()));
    ASSERT_EQ_INT(7, f.editor->view().guides.horizontal[0]);
    ASSERT_TRUE(!f.editor->history().canUndo());
}

TEST(without_rulers_a_drag_at_the_edge_draws_and_makes_no_guide)
{
    Fixture f(false);
    f.h.drag(CANVAS_X + 2.0f, 300, 500, 300);
    ASSERT_TRUE(f.editor->view().guides.empty());
}

TEST(hidden_rulers_do_not_take_a_guide_from_the_pencil)
{
    Fixture f(false);
    f.editor->view().guides.horizontal.push_back(4);
    const float y = f.view().pan_y + 4.0f * f.view().zoom;
    f.h.drag(500, y, 520, y);
    ASSERT_EQ_INT(4, f.editor->view().guides.horizontal[0]);
    ASSERT_TRUE(f.editor->history().canUndo());
}

int main()
{
    RUN(ticks_are_a_round_number_of_cels_and_stay_apart_on_screen);
    RUN(a_guide_snaps_to_the_nearest_cel_edge);
    RUN(a_guide_is_picked_only_within_reach_and_the_nearest_wins);
    RUN(dragging_from_the_top_ruler_makes_a_horizontal_guide);
    RUN(dragging_from_the_left_ruler_makes_a_vertical_guide);
    RUN(a_guide_dragged_back_onto_the_ruler_is_deleted);
    RUN(an_existing_guide_is_moved_by_dragging_it);
    RUN(without_rulers_a_drag_at_the_edge_draws_and_makes_no_guide);
    RUN(hidden_rulers_do_not_take_a_guide_from_the_pencil);
    return TEST_SUMMARY();
}
