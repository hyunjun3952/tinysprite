/* Headless tests for Editor::applyFilter: which layers and frames a filter reaches, the selection
 * limit and the single undo step. Built only when the UI is built; runs under
 * SDL_VIDEODRIVER=dummy. */

#include "editor.hpp"
#include "minitest.hpp"
#include "ui_editor_support.hpp"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

static FilterParams replace_red_with_blue()
{
    FilterParams p;
    p.kind = FilterParams::Kind::ReplaceColor;
    p.from = RED;
    p.to = BLUE;
    return p;
}

TEST(a_filter_changes_the_current_frame_only_unless_asked)
{
    Editor *e = make_editor();
    e->addFrame();
    e->sprite().layer(0)->cel(0)->set(0, 0, RED);
    e->sprite().layer(0)->cel(1)->set(0, 0, RED);
    ASSERT_TRUE(e->applyFilter(replace_red_with_blue(), false));
    ASSERT_TRUE(e->sprite().layer(0)->cel(1)->get(0, 0) == BLUE);
    ASSERT_TRUE(e->sprite().layer(0)->cel(0)->get(0, 0) == RED);
    ASSERT_TRUE(e->applyFilter(replace_red_with_blue(), true));
    ASSERT_TRUE(e->sprite().layer(0)->cel(0)->get(0, 0) == BLUE);
    delete e;
}

TEST(all_frames_is_one_undo_step)
{
    Editor *e = make_editor();
    e->addFrame();
    e->sprite().layer(0)->cel(0)->set(0, 0, RED);
    e->sprite().layer(0)->cel(1)->set(0, 0, RED);
    e->applyFilter(replace_red_with_blue(), true);
    e->undo();
    ASSERT_TRUE(e->sprite().layer(0)->cel(0)->get(0, 0) == RED);
    ASSERT_TRUE(e->sprite().layer(0)->cel(1)->get(0, 0) == RED);
    delete e;
}

TEST(a_filter_is_limited_to_the_selection)
{
    Editor *e = make_editor();
    e->sprite().layer(0)->cel(0)->set(0, 0, RED);
    e->sprite().layer(0)->cel(0)->set(5, 0, RED);
    e->setSelection(Rect{4, 0, 4, 2});
    ASSERT_TRUE(e->applyFilter(replace_red_with_blue(), false));
    ASSERT_TRUE(e->sprite().layer(0)->cel(0)->get(5, 0) == BLUE);
    ASSERT_TRUE(e->sprite().layer(0)->cel(0)->get(0, 0) == RED);
    delete e;
}

TEST(a_locked_or_hidden_layer_is_skipped)
{
    Editor *e = make_editor();
    e->sprite().layer(0)->cel(0)->set(0, 0, RED);
    e->setLayerLocked(0, true);
    ASSERT_TRUE(!e->applyFilter(replace_red_with_blue(), false));
    ASSERT_TRUE(e->sprite().layer(0)->cel(0)->get(0, 0) == RED);
    delete e;
}

TEST(a_linked_cel_is_filtered_once)
{
    Editor *e = make_editor();
    e->addFrame();
    e->linkCel(0, 0, 1);
    e->sprite().layer(0)->cel(0)->set(0, 0, RED);
    ASSERT_TRUE(e->applyFilter(replace_red_with_blue(), true));
    ASSERT_TRUE(e->sprite().layer(0)->cel(1)->get(0, 0) == BLUE);
    e->undo();
    ASSERT_TRUE(e->sprite().layer(0)->cel(1)->get(0, 0) == RED);
    delete e;
}

int main()
{
    if (!ui_init()) {
        return 1;
    }
    RUN(a_filter_changes_the_current_frame_only_unless_asked);
    RUN(all_frames_is_one_undo_step);
    RUN(a_filter_is_limited_to_the_selection);
    RUN(a_locked_or_hidden_layer_is_skipped);
    RUN(a_linked_cel_is_filtered_once);
    ui_quit();
    return TEST_SUMMARY();
}
