#include <string>
#include <utility>
#include <vector>

#include "minitest.hpp"
#include "tinysprite/mask.hpp"

/* Draws the mask as rows of '#' (in) and '.' (out) over a w x h area, for comparing by eye. */
static std::string rows_of(const Mask &mask, int w, int h)
{
    std::string out;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            out += mask.contains(x, y) ? '#' : '.';
        }
        out += y + 1 < h ? "/" : "";
    }
    return out;
}

/* An L: the left column of a 3 x 3 block and its bottom row. */
static Mask l_shape()
{
    return Mask::fromPlot(8, 8, [](const RasterPlotFn &plot) {
        plot(2, 2);
        plot(2, 3);
        plot(2, 4);
        plot(3, 4);
        plot(4, 4);
    });
}

TEST(a_full_mask_holds_every_pixel_of_its_rectangle_and_nothing_else)
{
    Mask mask = Mask::full(Rect{1, 2, 3, 2});
    ASSERT_TRUE(mask.contains(1, 2) && mask.contains(3, 3));
    ASSERT_TRUE(!mask.contains(0, 2) && !mask.contains(4, 2) && !mask.contains(1, 4));
    ASSERT_EQ_INT(6, mask.count());
    Rect b = mask.bounds();
    ASSERT_TRUE(b.x == 1 && b.y == 2 && b.w == 3 && b.h == 2);
}

TEST(a_mask_from_a_plot_is_trimmed_to_the_pixels_it_got)
{
    Mask mask = l_shape();
    Rect b = mask.bounds();
    ASSERT_TRUE(b.x == 2 && b.y == 2 && b.w == 3 && b.h == 3);
    ASSERT_EQ_INT(5, mask.count());
    ASSERT_TRUE(rows_of(mask, 6, 6) == "....../....../..#.../..#.../..###./......");
}

TEST(plotting_outside_the_area_is_ignored_and_nothing_plotted_is_an_empty_mask)
{
    Mask edge = Mask::fromPlot(4, 4, [](const RasterPlotFn &plot) {
        plot(-1, 0);
        plot(0, 4);
        plot(9, 9);
        plot(3, 3);
    });
    ASSERT_EQ_INT(1, edge.count());
    ASSERT_TRUE(edge.contains(3, 3));
    Mask none = Mask::fromPlot(4, 4, [](const RasterPlotFn &) {});
    ASSERT_TRUE(none.empty());
    ASSERT_TRUE(!none.contains(0, 0));
    ASSERT_TRUE(Mask().empty());
}

TEST(a_mask_moves_without_changing_its_shape)
{
    Mask moved = l_shape().translated(-2, 1);
    ASSERT_TRUE(rows_of(moved, 6, 6) == "....../....../....../#...../#...../###...");
    ASSERT_TRUE(moved.bounds().x == 0 && moved.bounds().y == 3);
}

TEST(a_mask_flips_and_turns_inside_its_own_rectangle)
{
    Mask l = l_shape(); /* "#.." / "#.." / "###" at (2, 2) */
    ASSERT_TRUE(rows_of(l.flipped(true), 6, 6) == "....../....../....#./....#./..###./......");
    ASSERT_TRUE(rows_of(l.flipped(false), 6, 6) == "....../....../..###./..#.../..#.../......");
    /* a quarter turn clockwise: the left column becomes the top row */
    ASSERT_TRUE(rows_of(l.rotated(true), 6, 6) == "....../....../..###./..#.../..#.../......");
    ASSERT_TRUE(rows_of(l.rotated(false), 6, 6) == "....../....../....#./....#./..###./......");
}

/* The same rule as rotating a selection's rectangle: the turned one shares the old one's center. */
TEST(a_rotated_mask_keeps_the_center_of_its_rectangle)
{
    Mask bar = Mask::full(Rect{2, 3, 4, 1});
    Rect b = bar.rotated(true).bounds();
    ASSERT_TRUE(b.x == 3 && b.y == 2 && b.w == 1 && b.h == 4);
    ASSERT_EQ_INT(4, bar.rotated(true).count());
}
TEST(masks_add_and_subtract)
{
    Mask a = Mask::full(Rect{0, 0, 3, 1});
    Mask b = Mask::full(Rect{2, 0, 3, 1});
    ASSERT_TRUE(rows_of(a.united(b), 6, 1) == "#####.");
    ASSERT_TRUE(rows_of(a.subtracted(b), 6, 1) == "##....");
    ASSERT_TRUE(a.subtracted(a).empty());
    ASSERT_TRUE(Mask().united(a).count() == 3);
}

TEST(a_mask_is_found_inside_a_cel_sized_grid_in_row_major_order)
{
    Mask mask = l_shape();
    std::vector<std::pair<int, int>> seen;
    mask.forEach([&](int x, int y) { seen.emplace_back(x, y); });
    ASSERT_EQ_INT(5, static_cast<int>(seen.size()));
    ASSERT_TRUE(seen.front() == std::make_pair(2, 2));
    ASSERT_TRUE(seen.back() == std::make_pair(4, 4));
}

/* A 6 x 3 cel: red on the left (x 0-1) and right (x 4-5), blue in the middle, a one-pixel hole of
 * a near-red at (5, 1). */
static Cel two_reds()
{
    Cel c(6, 3);
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 6; x++) {
            c.set(x, y, (x >= 2 && x <= 3) ? Rgba{0, 0, 255, 255} : Rgba{255, 0, 0, 255});
        }
    }
    c.set(5, 1, Rgba{250, 0, 0, 255});
    return c;
}

TEST(the_region_mask_is_the_connected_pixels_of_the_clicked_color)
{
    Cel c = two_reds();
    Mask left = mask_of_region(c, 0, 0, FillOptions());
    ASSERT_TRUE(rows_of(left, 6, 3) == "##..../##..../##....");
    Mask blue = mask_of_region(c, 2, 1, FillOptions());
    ASSERT_TRUE(rows_of(blue, 6, 3) == "..##../..##../..##..");
}

TEST(the_region_mask_can_take_every_pixel_of_the_color_and_close_colors)
{
    Cel c = two_reds();
    FillOptions everywhere;
    everywhere.contiguous = false;
    Mask reds = mask_of_region(c, 0, 0, everywhere);
    ASSERT_TRUE(rows_of(reds, 6, 3) ==
                "##..##/##..#./##..##"); /* the near red at (5, 1) is not exact */
    everywhere.tolerance = 10;
    ASSERT_TRUE(rows_of(mask_of_region(c, 0, 0, everywhere), 6, 3) == "##..##/##..##/##..##");
    ASSERT_TRUE(mask_of_region(c, -1, 0, FillOptions()).empty()); /* a click outside */
}

TEST(a_polygon_mask_is_the_inside_and_the_outline_clipped_to_the_cel)
{
    /* a right triangle with its corner at (1, 1) */
    Mask t = mask_of_polygon(8, 8, {{1, 1}, {5, 1}, {1, 5}});
    ASSERT_TRUE(t.contains(1, 1) && t.contains(5, 1) && t.contains(1, 5));
    ASSERT_TRUE(t.contains(2, 2) && !t.contains(5, 5) && !t.contains(0, 0));
    /* corners outside the cel: only the part inside is kept, no crash */
    Mask big = mask_of_polygon(4, 4, {{-5, -5}, {20, -5}, {20, 20}, {-5, 20}});
    ASSERT_EQ_INT(16, big.count());
    ASSERT_TRUE(mask_of_polygon(4, 4, {{1, 1}}).count() <= 1); /* a dot at most */
}

TEST(a_mask_is_clipped_to_an_area_and_masks_compare_by_their_pixels)
{
    Mask l = l_shape();                     /* "#.." / "#.." / "###" at (2, 2) */
    Mask top = l.clipped(Rect{0, 0, 8, 4}); /* drops the bottom row */
    ASSERT_EQ_INT(2, top.count());
    ASSERT_TRUE(top.bounds().h == 2);
    ASSERT_TRUE(l.clipped(Rect{6, 6, 2, 2}).empty());
    ASSERT_TRUE(l.clipped(Rect{0, 0, 8, 8}) == l);
    ASSERT_TRUE(l == l_shape());
    ASSERT_TRUE(!(l == l.translated(1, 0)));
    ASSERT_TRUE(!(l == l.flipped(true)));
    ASSERT_TRUE(Mask() == Mask());
    ASSERT_TRUE(l.flipped(true).flipped(true) == l);
}

int main()
{
    RUN(a_full_mask_holds_every_pixel_of_its_rectangle_and_nothing_else);
    RUN(a_mask_from_a_plot_is_trimmed_to_the_pixels_it_got);
    RUN(plotting_outside_the_area_is_ignored_and_nothing_plotted_is_an_empty_mask);
    RUN(a_mask_moves_without_changing_its_shape);
    RUN(a_mask_flips_and_turns_inside_its_own_rectangle);
    RUN(a_rotated_mask_keeps_the_center_of_its_rectangle);
    RUN(masks_add_and_subtract);
    RUN(a_mask_is_clipped_to_an_area_and_masks_compare_by_their_pixels);
    RUN(a_mask_is_found_inside_a_cel_sized_grid_in_row_major_order);
    RUN(the_region_mask_is_the_connected_pixels_of_the_clicked_color);
    RUN(the_region_mask_can_take_every_pixel_of_the_color_and_close_colors);
    RUN(a_polygon_mask_is_the_inside_and_the_outline_clipped_to_the_cel);
    return TEST_SUMMARY();
}
