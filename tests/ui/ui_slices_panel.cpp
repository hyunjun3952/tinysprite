/* The Slices section of the Inspector: the rules that keep a typed value valid (plain functions),
 * and a frame drawn over a real Editor. */
#include "editor.hpp"
#include "imgui.h"
#include "minitest.hpp"
#include "slices_panel.hpp"
#include "ui_harness.hpp"

TEST(a_typed_rectangle_is_kept_inside_the_sprite)
{
    Rect r = slice_rect_fit(Rect{-3, 2, 4, 4}, 16, 8);
    ASSERT_EQ_INT(0, r.x);
    ASSERT_EQ_INT(2, r.y);
    ASSERT_EQ_INT(4, r.w);
    r = slice_rect_fit(Rect{14, 6, 5, 5}, 16, 8); /* sticks out: moved in, not shrunk */
    ASSERT_EQ_INT(11, r.x);
    ASSERT_EQ_INT(3, r.y);
    ASSERT_EQ_INT(5, r.w);
    r = slice_rect_fit(Rect{0, 0, 99, 0}, 16, 8); /* too big and too small: limited */
    ASSERT_EQ_INT(16, r.w);
    ASSERT_EQ_INT(1, r.h);
}

TEST(a_pivot_is_kept_inside_its_slice)
{
    Sprite::Slice slice;
    slice.rect = Rect{2, 2, 4, 3};
    slice.hasPivot = true;
    slice.pivotX = 9;
    slice.pivotY = -2;
    slice_pivot_fit(slice);
    ASSERT_EQ_INT(3, slice.pivotX);
    ASSERT_EQ_INT(0, slice.pivotY);
}

TEST(a_new_pivot_starts_at_the_middle_of_the_slice)
{
    Sprite::Slice slice;
    slice.rect = Rect{0, 0, 8, 5};
    slice_pivot_center(slice);
    ASSERT_TRUE(slice.hasPivot);
    ASSERT_EQ_INT(4, slice.pivotX);
    ASSERT_EQ_INT(2, slice.pivotY);
}

TEST(the_panel_draws_with_and_without_slices)
{
    test_ui::Harness h;
    Editor *e = Editor::create(h.renderer, 16, 16).release();
    SlicesPanel panel(*e);
    h.body = [&] {
        ImGui::SetNextWindowPos(ImVec2(100.0f, 100.0f));
        ImGui::SetNextWindowSize(ImVec2(300.0f, 400.0f));
        ImGui::Begin("##slices_rig");
        panel.show();
        ImGui::End();
    };
    h.frame(0.0f, 0.0f, false); /* none */
    e->addSlice(Rect{1, 1, 4, 4});
    e->addSlice(Rect{6, 6, 3, 3});
    h.frame(0.0f, 0.0f, false); /* a list and the details of the selected one */
    e->selectSlice(-1);
    h.frame(0.0f, 0.0f, false);
    ASSERT_EQ_INT(2, static_cast<int>(e->sprite().slices().size()));
    delete e;
}

int main()
{
    RUN(a_typed_rectangle_is_kept_inside_the_sprite);
    RUN(a_pivot_is_kept_inside_its_slice);
    RUN(a_new_pivot_starts_at_the_middle_of_the_slice);
    RUN(the_panel_draws_with_and_without_slices);
    return TEST_SUMMARY();
}
