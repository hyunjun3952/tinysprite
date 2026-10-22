#include <climits>

#include "minitest.hpp"
#include "tinysprite/rect.hpp"

TEST(clip_keeps_a_rectangle_that_fits)
{
    Rect r = rect_clip(Rect{1, 2, 3, 4}, 10, 10);
    ASSERT_EQ_INT(1, r.x);
    ASSERT_EQ_INT(2, r.y);
    ASSERT_EQ_INT(3, r.w);
    ASSERT_EQ_INT(4, r.h);
}

TEST(clip_trims_every_side)
{
    Rect r = rect_clip(Rect{-2, -3, 20, 20}, 8, 6);
    ASSERT_EQ_INT(0, r.x);
    ASSERT_EQ_INT(0, r.y);
    ASSERT_EQ_INT(8, r.w);
    ASSERT_EQ_INT(6, r.h);
}

TEST(clip_shrinks_by_what_hangs_over_the_left_and_top)
{
    Rect r = rect_clip(Rect{-1, -1, 3, 3}, 8, 8);
    ASSERT_EQ_INT(0, r.x);
    ASSERT_EQ_INT(0, r.y);
    ASSERT_EQ_INT(2, r.w);
    ASSERT_EQ_INT(2, r.h);
}

TEST(clip_reports_an_empty_result_as_zero_size)
{
    ASSERT_TRUE(rect_is_empty(rect_clip(Rect{10, 0, 3, 3}, 8, 8)));
    ASSERT_TRUE(rect_is_empty(rect_clip(Rect{0, -5, 3, 3}, 8, 8)));
    ASSERT_TRUE(rect_is_empty(rect_clip(Rect{0, 0, 0, 3}, 8, 8)));
    ASSERT_TRUE(rect_is_empty(rect_clip(Rect{0, 0, 3, -1}, 8, 8)));
}

TEST(clip_does_not_overflow_on_huge_extents)
{
    Rect r = rect_clip(Rect{INT_MAX - 1, 0, INT_MAX, 1}, 8, 8);
    ASSERT_TRUE(rect_is_empty(r));
    r = rect_clip(Rect{INT_MIN, 0, INT_MAX, 1}, 8, 8);
    ASSERT_TRUE(rect_is_empty(r));
    r = rect_clip(Rect{0, 0, INT_MAX, INT_MAX}, 8, 6);
    ASSERT_EQ_INT(8, r.w);
    ASSERT_EQ_INT(6, r.h);
}

TEST(contains_is_half_open)
{
    Rect r = {2, 3, 4, 2};
    ASSERT_TRUE(rect_contains(r, 2, 3));
    ASSERT_TRUE(rect_contains(r, 5, 4));
    ASSERT_TRUE(!rect_contains(r, 6, 4));
    ASSERT_TRUE(!rect_contains(r, 5, 5));
    ASSERT_TRUE(!rect_contains(r, 1, 3));
}

TEST(from_corners_orders_and_includes_both_ends)
{
    Rect r = rect_from_corners(5, 1, 2, 4);
    ASSERT_EQ_INT(2, r.x);
    ASSERT_EQ_INT(1, r.y);
    ASSERT_EQ_INT(4, r.w);
    ASSERT_EQ_INT(4, r.h);
}

TEST(union_covers_both)
{
    Rect r = rect_union(Rect{0, 0, 2, 2}, Rect{5, 6, 2, 2});
    ASSERT_EQ_INT(0, r.x);
    ASSERT_EQ_INT(0, r.y);
    ASSERT_EQ_INT(7, r.w);
    ASSERT_EQ_INT(8, r.h);
}

TEST(grow_adds_the_amount_on_every_side)
{
    Rect r = rect_grow(Rect{4, 5, 2, 3}, 2);
    ASSERT_EQ_INT(2, r.x);
    ASSERT_EQ_INT(3, r.y);
    ASSERT_EQ_INT(6, r.w);
    ASSERT_EQ_INT(7, r.h);
}

int main()
{
    RUN(clip_keeps_a_rectangle_that_fits);
    RUN(clip_trims_every_side);
    RUN(clip_shrinks_by_what_hangs_over_the_left_and_top);
    RUN(clip_reports_an_empty_result_as_zero_size);
    RUN(clip_does_not_overflow_on_huge_extents);
    RUN(contains_is_half_open);
    RUN(from_corners_orders_and_includes_both_ends);
    RUN(union_covers_both);
    RUN(grow_adds_the_amount_on_every_side);
    return TEST_SUMMARY();
}
