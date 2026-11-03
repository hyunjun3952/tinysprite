/* The rulers and the guides: the tick spacing, picking a guide, and the canvas creating, moving and
 * deleting guides with the mouse. Runs under SDL_VIDEODRIVER=dummy. */
#include <cmath>
#include <memory>
#include <optional>

#include "canvas.hpp"
#include "editor.hpp"
#include "minitest.hpp"
#include "ruler.hpp"
#include "theme.hpp"
#include "tinysprite/io.hpp"
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
    ASSERT_EQ_INT(2, ruler_step(64.0f)); /* 1 cel is 64 px, under the 72 px a label needs */
    ASSERT_EQ_INT(1, ruler_step(72.0f));
    ASSERT_EQ_INT(5, ruler_step(16.0f));
    ASSERT_EQ_INT(20, ruler_step(4.0f));
    ASSERT_EQ_INT(100, ruler_step(1.0f));
    ASSERT_EQ_INT(2000, ruler_step(0.0625f));
    ASSERT_EQ_INT(200, ruler_step(1.0f, 150.0f)); /* a wider gap, as at a bigger UI scale */
}

TEST(toggling_the_rulers_keeps_the_sprite_where_it_is)
{
    Fixture f;
    f.editor->view().auto_fit = false; /* a view the user set, not the fitted one */
    const float left = CANVAS_X + f.ruler + f.view().pan_x;
    const float top = f.ruler + f.view().pan_y;
    f.editor->overlays().rulers = false;
    f.h.frame(400, 300, false);
    f.h.frame(400, 300, false);
    ASSERT_TRUE(std::fabs(CANVAS_X + f.view().pan_x - left) < 0.5f);
    ASSERT_TRUE(std::fabs(f.view().pan_y - top) < 0.5f);
    f.editor->overlays().rulers = true;
    f.h.frame(400, 300, false);
    f.h.frame(400, 300, false);
    ASSERT_TRUE(std::fabs(CANVAS_X + f.ruler + f.view().pan_x - left) < 0.5f);
    ASSERT_TRUE(std::fabs(f.ruler + f.view().pan_y - top) < 0.5f);
}

TEST(a_far_pan_and_a_tiny_zoom_still_draw_the_rulers)
{
    Fixture f;
    f.editor->view().viewport.pan_x = -3.0e12f;
    f.editor->view().viewport.pan_y = 3.0e12f;
    f.editor->view().viewport.zoom = 0.0625f;
    f.h.frame(400, 300, false);
    f.h.frame(400, 300, false);
    ASSERT_TRUE(f.editor->guides().empty());
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
    ASSERT_EQ_INT(1, static_cast<int>(f.editor->guides().horizontal.size()));
    ASSERT_EQ_INT(f.edgeAtY(300), f.editor->guides().horizontal[0]);
    ASSERT_TRUE(f.editor->guides().vertical.empty());
    ASSERT_TRUE(f.editor->history().canUndo()); /* the whole drag is one undo step */
    ASSERT_TRUE(!f.editor->modifiedOutsideHistory());
    std::optional<Sprite> saved = pxa_decode(pxa_encode(f.editor->sprite()));
    ASSERT_TRUE(saved.has_value());
    if (saved) {
        ASSERT_EQ_INT(1, static_cast<int>(saved->guides().horizontal.size()));
    }
}

TEST(dragging_from_the_left_ruler_makes_a_vertical_guide)
{
    Fixture f;
    f.h.drag(CANVAS_X + f.ruler / 2.0f, 300, 500, 300);
    ASSERT_EQ_INT(1, static_cast<int>(f.editor->guides().vertical.size()));
    ASSERT_EQ_INT(f.edgeAtX(500), f.editor->guides().vertical[0]);
    ASSERT_TRUE(f.editor->guides().horizontal.empty());
}

TEST(a_guide_dragged_back_onto_the_ruler_is_deleted)
{
    Fixture f;
    f.h.drag(500, f.ruler / 2.0f, 500, 300);
    const float y = f.ruler + f.view().pan_y +
                    static_cast<float>(f.editor->guides().horizontal[0]) * f.view().zoom;
    f.h.drag(500, y, 500, f.ruler / 2.0f);
    ASSERT_TRUE(f.editor->guides().empty());
    f.editor->undo(); /* the delete is a step of its own: the guide comes back where it was */
    ASSERT_EQ_INT(1, static_cast<int>(f.editor->guides().horizontal.size()));
}

TEST(an_existing_guide_is_moved_by_dragging_it)
{
    Fixture f;
    f.editor->guides().horizontal.push_back(4);
    const float y = f.ruler + f.view().pan_y + 4.0f * f.view().zoom;
    f.h.drag(500, y, 500, y + 3.0f * f.view().zoom);
    ASSERT_EQ_INT(1, static_cast<int>(f.editor->guides().horizontal.size()));
    ASSERT_EQ_INT(7, f.editor->guides().horizontal[0]);
    f.editor->undo();
    ASSERT_EQ_INT(4, f.editor->guides().horizontal[0]);
    f.editor->redo();
    ASSERT_EQ_INT(7, f.editor->guides().horizontal[0]);
}

TEST(a_new_guide_is_undone_and_redone_with_the_drag_that_made_it)
{
    Fixture f;
    f.h.drag(500, f.ruler / 2.0f, 500, 300);
    const int edge = f.editor->guides().horizontal[0];
    f.editor->undo();
    ASSERT_TRUE(f.editor->guides().empty());
    ASSERT_TRUE(!f.editor->history().canUndo());
    f.editor->redo();
    ASSERT_EQ_INT(1, static_cast<int>(f.editor->guides().horizontal.size()));
    ASSERT_EQ_INT(edge, f.editor->guides().horizontal[0]);
}

TEST(a_click_on_a_guide_that_does_not_move_it_records_nothing)
{
    Fixture f;
    f.editor->guides().horizontal.push_back(4);
    const float y = f.ruler + f.view().pan_y + 4.0f * f.view().zoom;
    const uint64_t revision = f.editor->documentRevision();
    f.h.drag(500, y, 500, y);
    ASSERT_EQ_INT(4, f.editor->guides().horizontal[0]);
    ASSERT_TRUE(!f.editor->history().canUndo());
    ASSERT_TRUE(f.editor->documentRevision() == revision);
}

TEST(clearing_the_guides_is_one_step_that_undo_brings_back)
{
    Fixture f;
    f.editor->guides().horizontal.push_back(4);
    f.editor->guides().vertical.push_back(2);
    f.editor->clearGuides();
    ASSERT_TRUE(f.editor->guides().empty());
    ASSERT_TRUE(f.editor->history().canUndo());
    f.editor->undo();
    ASSERT_EQ_INT(4, f.editor->guides().horizontal[0]);
    ASSERT_EQ_INT(2, f.editor->guides().vertical[0]);
}

TEST(guides_changed_during_a_stroke_are_kept_and_count_as_a_change_outside_the_history)
{
    Fixture f;
    f.editor->history().beginStroke(*f.editor->sprite().layer(0)->cel(0));
    f.editor->guides().horizontal.push_back(3);
    f.editor->commitGuides(Guides{});
    ASSERT_EQ_INT(1, static_cast<int>(f.editor->guides().horizontal.size()));
    if (!f.editor->guides().horizontal.empty()) {
        ASSERT_EQ_INT(3, f.editor->guides().horizontal[0]);
    }
    ASSERT_TRUE(f.editor->modifiedOutsideHistory());
    f.editor->history().cancelStroke();
    ASSERT_TRUE(!f.editor->history().canUndo());
}

TEST(clearing_no_guides_records_nothing)
{
    Fixture f;
    f.editor->clearGuides();
    ASSERT_TRUE(!f.editor->history().canUndo());
}

TEST(without_rulers_a_drag_at_the_edge_draws_and_makes_no_guide)
{
    Fixture f(false);
    f.h.drag(CANVAS_X + 2.0f, 300, 500, 300);
    ASSERT_TRUE(f.editor->guides().empty());
}

TEST(hidden_rulers_do_not_take_a_guide_from_the_pencil)
{
    Fixture f(false);
    f.editor->guides().horizontal.push_back(4);
    const float y = f.view().pan_y + 4.0f * f.view().zoom;
    f.h.drag(500, y, 520, y);
    ASSERT_EQ_INT(4, f.editor->guides().horizontal[0]);
    ASSERT_TRUE(f.editor->history().canUndo());
}

int main()
{
    RUN(ticks_are_a_round_number_of_cels_and_stay_apart_on_screen);
    RUN(toggling_the_rulers_keeps_the_sprite_where_it_is);
    RUN(a_far_pan_and_a_tiny_zoom_still_draw_the_rulers);
    RUN(a_guide_snaps_to_the_nearest_cel_edge);
    RUN(a_guide_is_picked_only_within_reach_and_the_nearest_wins);
    RUN(dragging_from_the_top_ruler_makes_a_horizontal_guide);
    RUN(dragging_from_the_left_ruler_makes_a_vertical_guide);
    RUN(a_guide_dragged_back_onto_the_ruler_is_deleted);
    RUN(an_existing_guide_is_moved_by_dragging_it);
    RUN(a_new_guide_is_undone_and_redone_with_the_drag_that_made_it);
    RUN(a_click_on_a_guide_that_does_not_move_it_records_nothing);
    RUN(clearing_the_guides_is_one_step_that_undo_brings_back);
    RUN(guides_changed_during_a_stroke_are_kept_and_count_as_a_change_outside_the_history);
    RUN(clearing_no_guides_records_nothing);
    RUN(without_rulers_a_drag_at_the_edge_draws_and_makes_no_guide);
    RUN(hidden_rulers_do_not_take_a_guide_from_the_pencil);
    return TEST_SUMMARY();
}
