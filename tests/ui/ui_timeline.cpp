/* The timeline driven with synthetic input: cels select their frame and layer, frame numbers move
 * frames, and the buttons edit them. */
#include <memory>

#include "editor.h"
#include "minitest.h"
#include "timeline_panel.h"
#include "ui_harness.h"

using test_ui::Harness;

namespace
{

struct Fixture {
    Harness h;
    std::unique_ptr<Editor> editor;
    std::unique_ptr<TimelinePanel> timeline;

    Fixture()
    {
        editor = Editor::create(h.renderer, 8, 8);
        timeline = std::make_unique<TimelinePanel>(*editor);
        h.body = [this] { timeline->show(0.0f, 500.0f, 900.0f); };
        h.frame(0.0f, 0.0f, false);
        h.frame(0.0f, 0.0f, false);
    }

    static ImVec2 center(TimelinePanel::Rect r)
    {
        return ImVec2((r.min.x + r.max.x) / 2.0f, (r.min.y + r.max.y) / 2.0f);
    }
    void click(ImVec2 at)
    {
        h.frame(at.x, at.y, false);
        h.frame(at.x, at.y, true);
        h.frame(at.x, at.y, false);
        h.frame(at.x, at.y, false);
    }
};

} // namespace

TEST(the_grid_has_a_cel_per_layer_and_frame)
{
    Fixture f;
    f.editor->addFrame();
    f.editor->addLayer();
    f.h.frame(0.0f, 0.0f, false);
    for (int l = 0; l < 2; l++) {
        for (int fr = 0; fr < 2; fr++) {
            TimelinePanel::Rect r = f.timeline->cellRect(l, fr);
            ASSERT_TRUE(r.max.x > r.min.x && r.max.y > r.min.y);
        }
    }
    /* the top layer is the first row */
    ASSERT_TRUE(f.timeline->cellRect(1, 0).min.y < f.timeline->cellRect(0, 0).min.y);
}

TEST(clicking_a_cel_selects_its_frame_and_layer)
{
    Fixture f;
    f.editor->addFrame();
    f.editor->addFrame();
    f.editor->addLayer();
    f.editor->setFrame(0);
    f.editor->setActiveLayer(1);
    f.h.frame(0.0f, 0.0f, false);
    f.click(Fixture::center(f.timeline->cellRect(0, 2)));
    ASSERT_EQ_INT(2, f.editor->frame());
    ASSERT_EQ_INT(0, f.editor->activeLayer());
}

TEST(clicking_a_frame_number_selects_the_frame)
{
    Fixture f;
    f.editor->addFrame();
    f.editor->setFrame(0);
    f.h.frame(0.0f, 0.0f, false);
    f.click(Fixture::center(f.timeline->headerRect(1)));
    ASSERT_EQ_INT(1, f.editor->frame());
}

TEST(dragging_a_frame_number_moves_the_frame)
{
    Fixture f;
    f.editor->addFrame();
    f.editor->addFrame();
    f.editor->setFrame(0);
    f.editor->sprite().layer(0)->cel(0)->set(0, 0, Rgba{255, 0, 0, 255});
    f.editor->markAllDirty();
    f.h.frame(0.0f, 0.0f, false);
    ImVec2 from = Fixture::center(f.timeline->headerRect(0));
    ImVec2 to = Fixture::center(f.timeline->headerRect(2));
    f.h.drag(from.x, from.y, to.x, to.y);
    ASSERT_EQ_INT(255, f.editor->sprite().layer(0)->cel(2)->get(0, 0).a);
    ASSERT_EQ_INT(0, f.editor->sprite().layer(0)->cel(0)->get(0, 0).a);
}

TEST(a_dot_marks_a_cel_with_pixels)
{
    Fixture f;
    f.editor->addFrame();
    f.editor->sprite().layer(0)->cel(1)->set(3, 3, Rgba{9, 9, 9, 255});
    f.editor->markAllDirty();
    f.h.frame(0.0f, 0.0f, false);
    TimelinePanel::Rect empty = f.timeline->cellRect(0, 0);
    TimelinePanel::Rect full = f.timeline->cellRect(0, 1);
    ImVec2 e = Fixture::center(empty);
    ImVec2 d = Fixture::center(full);
    /* the dot is lighter than the cell behind it, which is plain */
    ASSERT_TRUE(f.h.pixel(static_cast<int>(d.x), static_cast<int>(d.y)) !=
                f.h.pixel(static_cast<int>(e.x), static_cast<int>(e.y)));
}

TEST(clicking_a_tag_band_selects_the_tag_and_again_deselects_it)
{
    Fixture f;
    f.editor->addFrame();
    f.editor->addFrame();
    f.editor->setFrame(1);
    f.editor->addTag(); /* frame 2 */
    f.editor->selectTag(-1);
    f.h.frame(0.0f, 0.0f, false);
    f.click(Fixture::center(f.timeline->tagCellRect(1)));
    ASSERT_EQ_INT(0, f.editor->selectedTag());
    f.click(Fixture::center(f.timeline->tagCellRect(1)));
    ASSERT_EQ_INT(-1, f.editor->selectedTag());
}

TEST(clicking_a_band_with_no_tag_selects_nothing)
{
    Fixture f;
    f.editor->addFrame();
    f.editor->addTag();
    f.editor->selectTag(-1);
    f.h.frame(0.0f, 0.0f, false);
    f.click(Fixture::center(f.timeline->tagCellRect(0)));
    ASSERT_EQ_INT(-1, f.editor->selectedTag());
}

TEST(the_edit_row_draws_for_a_selected_tag)
{
    Fixture f;
    f.h.frame(0.0f, 0.0f, false);
    ASSERT_EQ_INT(0, static_cast<int>(f.editor->sprite().tags().size()));
    f.editor->addTag();
    f.h.frame(0.0f, 0.0f, false);
    ASSERT_EQ_INT(0, f.editor->selectedTag());
    f.h.frame(0.0f, 0.0f, false); /* the edit row is drawn for the selected tag without asserting */
}

int main(void)
{
    RUN(the_grid_has_a_cel_per_layer_and_frame);
    RUN(clicking_a_cel_selects_its_frame_and_layer);
    RUN(clicking_a_frame_number_selects_the_frame);
    RUN(dragging_a_frame_number_moves_the_frame);
    RUN(a_dot_marks_a_cel_with_pixels);
    RUN(clicking_a_tag_band_selects_the_tag_and_again_deselects_it);
    RUN(clicking_a_band_with_no_tag_selects_nothing);
    RUN(the_edit_row_draws_for_a_selected_tag);
    return TEST_SUMMARY();
}
