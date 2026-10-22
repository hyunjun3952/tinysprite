#include "minitest.hpp"
#include "tinysprite/symmetry.hpp"

TEST(copy_count_follows_the_enabled_axes)
{
    ASSERT_EQ_INT(1, symmetry_count(Symmetry{}));
    ASSERT_EQ_INT(2, symmetry_count(Symmetry{true, false}));
    ASSERT_EQ_INT(2, symmetry_count(Symmetry{false, true}));
    ASSERT_EQ_INT(4, symmetry_count(Symmetry{true, true}));
}

TEST(copy_zero_is_the_point_itself)
{
    ToolPoint p = symmetry_copy(Symmetry{true, true}, 8, 6, ToolPoint{1, 2}, 0);
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
    ToolPoint p = symmetry_copy(Symmetry{false, true}, 8, 6, ToolPoint{1, 2}, 1);
    ASSERT_EQ_INT(1, p.x);
    ASSERT_EQ_INT(3, p.y);
}

TEST(points_skips_reflections_that_land_on_an_earlier_copy)
{
    ASSERT_EQ_INT(
        4, static_cast<int>(symmetry_points(Symmetry{true, true}, 8, 6, ToolPoint{1, 1}).size()));
    /* x = 3 is the center column of a 7-wide cel */
    ASSERT_EQ_INT(
        2, static_cast<int>(symmetry_points(Symmetry{true, true}, 7, 6, ToolPoint{3, 1}).size()));
    ASSERT_EQ_INT(
        1, static_cast<int>(symmetry_points(Symmetry{true, true}, 7, 7, ToolPoint{3, 3}).size()));
    std::vector<ToolPoint> out = symmetry_points(Symmetry{}, 8, 6, ToolPoint{1, 1});
    ASSERT_EQ_INT(1, static_cast<int>(out.size()));
    ASSERT_EQ_INT(1, out[0].x);
    ASSERT_EQ_INT(1, out[0].y);
}

int main()
{
    RUN(copy_count_follows_the_enabled_axes);
    RUN(copy_zero_is_the_point_itself);
    RUN(copies_reflect_across_the_cel_center);
    RUN(a_single_axis_copy_is_that_axis_only);
    RUN(points_skips_reflections_that_land_on_an_earlier_copy);
    return TEST_SUMMARY();
}
