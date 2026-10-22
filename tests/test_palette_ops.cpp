#include "minitest.hpp"
#include "tinysprite/palette_ops.hpp"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

static Palette two()
{
    Palette p;
    p.add(RED);
    p.add(BLUE);
    return p;
}

TEST(palettes_compare_by_their_colors_in_order)
{
    ASSERT_TRUE(two() == two());
    Palette other;
    other.add(BLUE);
    other.add(RED);
    ASSERT_TRUE(!(two() == other));
    Palette shorter;
    shorter.add(RED);
    ASSERT_TRUE(two() != shorter);
}

TEST(replacing_the_palette_is_one_undo_step)
{
    Sprite s(2, 2);
    History h;
    s.palette() = two();
    Palette next;
    next.add(Rgba{1, 2, 3, 255});
    ASSERT_TRUE(palette_op_set(h, s, next));
    ASSERT_EQ_INT(1, s.palette().count());
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(s.palette() == two());
    ASSERT_TRUE(h.redo());
    ASSERT_TRUE(s.palette() == next);
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(!h.canUndo());
}

TEST(setting_the_same_palette_records_nothing)
{
    Sprite s(2, 2);
    History h;
    s.palette() = two();
    ASSERT_TRUE(palette_op_set(h, s, two()));
    ASSERT_TRUE(!h.canUndo());
}

TEST(a_stroke_in_progress_refuses_the_change)
{
    Sprite s(2, 2);
    History h;
    h.beginStroke(*s.layer(0)->cel(0));
    ASSERT_TRUE(!palette_op_set(h, s, two()));
    h.cancelStroke();
    ASSERT_EQ_INT(0, s.palette().count());
    ASSERT_TRUE(!h.canUndo());
}

TEST(pixel_steps_and_palette_steps_interleave)
{
    Sprite s(2, 2);
    History h;
    h.beginStroke(*s.layer(0)->cel(0));
    h.setPixel(0, 0, RED);
    h.endStroke();
    palette_op_set(h, s, two());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, s.palette().count());
    ASSERT_EQ_INT(255, s.layer(0)->cel(0)->get(0, 0).a);
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, s.layer(0)->cel(0)->get(0, 0).a);
}

int main()
{
    RUN(palettes_compare_by_their_colors_in_order);
    RUN(replacing_the_palette_is_one_undo_step);
    RUN(setting_the_same_palette_records_nothing);
    RUN(a_stroke_in_progress_refuses_the_change);
    RUN(pixel_steps_and_palette_steps_interleave);
    return TEST_SUMMARY();
}
