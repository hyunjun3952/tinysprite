#include "minitest.h"
#include "tinysprite/symmetry.h"

TEST(copy_count_follows_the_enabled_axes)
{
    ASSERT_EQ_INT(1, symmetry_count((Symmetry){0}));
    ASSERT_EQ_INT(2, symmetry_count((Symmetry){.mirror_x = true}));
    ASSERT_EQ_INT(2, symmetry_count((Symmetry){.mirror_y = true}));
    ASSERT_EQ_INT(4, symmetry_count((Symmetry){true, true}));
}

TEST(copy_zero_is_the_point_itself)
{
    ToolPoint p = symmetry_copy((Symmetry){true, true}, 8, 6, (ToolPoint){1, 2}, 0);
    ASSERT_EQ_INT(1, p.x);
    ASSERT_EQ_INT(2, p.y);
}

TEST(copies_reflect_across_the_cel_center)
{
    Symmetry both = {true, true};
    ToolPoint at = {1, 2};
    ToolPoint x = symmetry_copy(both, 8, 6, at, 1);
    ToolPoint y = symmetry_copy(both, 8, 6, at, 2);
    ToolPoint xy = symmetry_copy(both, 8, 6, at, 3);
    ASSERT_EQ_INT(6, x.x);
    ASSERT_EQ_INT(2, x.y);
    ASSERT_EQ_INT(1, y.x);
    ASSERT_EQ_INT(3, y.y);
    ASSERT_EQ_INT(6, xy.x);
    ASSERT_EQ_INT(3, xy.y);
}

TEST(a_single_axis_copy_is_that_axis_only)
{
    ToolPoint p = symmetry_copy((Symmetry){.mirror_y = true}, 8, 6, (ToolPoint){1, 2}, 1);
    ASSERT_EQ_INT(1, p.x);
    ASSERT_EQ_INT(3, p.y);
}

TEST(points_skips_reflections_that_land_on_an_earlier_copy)
{
    ToolPoint out[4];
    ASSERT_EQ_INT(4, symmetry_points((Symmetry){true, true}, 8, 6, (ToolPoint){1, 1}, out));
    /* x = 3 is the center column of a 7-wide cel */
    ASSERT_EQ_INT(2, symmetry_points((Symmetry){true, true}, 7, 6, (ToolPoint){3, 1}, out));
    ASSERT_EQ_INT(1, symmetry_points((Symmetry){true, true}, 7, 7, (ToolPoint){3, 3}, out));
    ASSERT_EQ_INT(1, symmetry_points((Symmetry){0}, 8, 6, (ToolPoint){1, 1}, out));
    ASSERT_EQ_INT(1, out[0].x);
    ASSERT_EQ_INT(1, out[0].y);
}

int main(void)
{
    RUN(copy_count_follows_the_enabled_axes);
    RUN(copy_zero_is_the_point_itself);
    RUN(copies_reflect_across_the_cel_center);
    RUN(a_single_axis_copy_is_that_axis_only);
    RUN(points_skips_reflections_that_land_on_an_earlier_copy);
    return TEST_SUMMARY();
}
