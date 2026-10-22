#include "minitest.hpp"
#include "tinysprite/slice_ops.hpp"

static Sprite::Slice slice(const char *name, Rect rect)
{
    Sprite::Slice s;
    s.name = name;
    s.rect = rect;
    return s;
}

TEST(a_slice_is_a_named_rectangle_with_an_optional_pivot)
{
    Sprite s(16, 16);
    ASSERT_EQ_INT(0, static_cast<int>(s.slices().size()));
    Sprite::Slice hit = slice("hitbox", Rect{2, 3, 4, 5});
    hit.hasPivot = true;
    hit.pivotX = 1;
    hit.pivotY = 4;
    ASSERT_TRUE(s.addSlice(hit));
    ASSERT_TRUE(s.slices()[0].name == "hitbox");
    ASSERT_EQ_INT(4, s.slices()[0].rect.w);
    ASSERT_TRUE(s.slices()[0].hasPivot);
    ASSERT_EQ_INT(4, s.slices()[0].pivotY);
}

TEST(bad_slices_are_refused)
{
    Sprite s(8, 8);
    ASSERT_TRUE(!s.addSlice(slice("a", Rect{0, 0, 0, 1})));
    ASSERT_TRUE(!s.addSlice(slice("a", Rect{-1, 0, 2, 2})));
    ASSERT_TRUE(!s.addSlice(slice("a", Rect{7, 7, 2, 2})));
    Sprite::Slice pivot = slice("a", Rect{0, 0, 2, 2});
    pivot.hasPivot = true;
    pivot.pivotX =
        3; /* the pivot is a point on the slice's grid: 0..width, the far edge included */
    ASSERT_TRUE(!s.addSlice(pivot));
    pivot.pivotX = 2;
    ASSERT_TRUE(s.addSlice(pivot));
    ASSERT_TRUE(s.removeSlice(0));
    ASSERT_TRUE(!s.addSlice(slice("", Rect{0, 0, 1, 1})));
    ASSERT_EQ_INT(0, static_cast<int>(s.slices().size()));
    ASSERT_TRUE(s.addSlice(slice("ok", Rect{6, 6, 2, 2})));
    ASSERT_TRUE(!s.setSlice(1, slice("x", Rect{0, 0, 1, 1})));
    ASSERT_TRUE(!s.removeSlice(1));
    ASSERT_TRUE(s.removeSlice(0));
}

TEST(a_long_name_is_cut)
{
    Sprite s(4, 4);
    ASSERT_TRUE(s.addSlice(slice(std::string(100, 'x').c_str(), Rect{0, 0, 1, 1})));
    ASSERT_EQ_INT(SLICE_NAME_MAX, static_cast<int>(s.slices()[0].name.size()));
}

TEST(slice_ops_undo_and_redo_each_edit)
{
    Sprite s(8, 8);
    History h;
    ASSERT_TRUE(slice_op_add(h, s, slice("a", Rect{0, 0, 2, 2})));
    ASSERT_TRUE(slice_op_add(h, s, slice("b", Rect{4, 4, 2, 2})));
    ASSERT_TRUE(slice_op_set(h, s, 0, slice("c", Rect{1, 1, 3, 3})));
    ASSERT_TRUE(s.slices()[0].name == "c");
    h.undo();
    ASSERT_TRUE(s.slices()[0].name == "a");
    ASSERT_TRUE(slice_op_remove(h, s, 0));
    ASSERT_EQ_INT(1, static_cast<int>(s.slices().size()));
    ASSERT_TRUE(s.slices()[0].name == "b");
    h.undo();
    ASSERT_EQ_INT(2, static_cast<int>(s.slices().size()));
    h.redo();
    ASSERT_EQ_INT(1, static_cast<int>(s.slices().size()));
}

TEST(setting_a_slice_to_what_it_is_records_nothing)
{
    Sprite s(8, 8);
    History h;
    slice_op_add(h, s, slice("a", Rect{0, 0, 2, 2}));
    h.undo();
    h.redo();
    ASSERT_TRUE(slice_op_set(h, s, 0, slice("a", Rect{0, 0, 2, 2})));
    h.undo();
    ASSERT_EQ_INT(0, static_cast<int>(s.slices().size())); /* only the add was a step */
}

TEST(slice_ops_refuse_bad_input)
{
    Sprite s(8, 8);
    History h;
    ASSERT_TRUE(!slice_op_add(h, s, slice("a", Rect{0, 0, 9, 1})));
    ASSERT_TRUE(!slice_op_remove(h, s, 0));
    ASSERT_TRUE(!h.canUndo());
}

int main()
{
    RUN(a_slice_is_a_named_rectangle_with_an_optional_pivot);
    RUN(bad_slices_are_refused);
    RUN(a_long_name_is_cut);
    RUN(slice_ops_undo_and_redo_each_edit);
    RUN(setting_a_slice_to_what_it_is_records_nothing);
    RUN(slice_ops_refuse_bad_input);
    return TEST_SUMMARY();
}
