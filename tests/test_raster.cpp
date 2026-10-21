#include <string.h>

#include "minitest.h"
#include "tinysprite/raster.h"

#define GRID_W 12
#define GRID_H 12

/* Collects plotted pixels into a character grid so results compare against golden rows. */
typedef struct Grid {
    char px[GRID_H][GRID_W + 1];
    int plots;
    int repeats;
    int outside;
} Grid;

static void grid_init(Grid *g)
{
    memset(g, 0, sizeof(*g));
    for (int y = 0; y < GRID_H; y++) {
        memset(g->px[y], '.', GRID_W);
    }
}

static RasterPlotFn grid_plot(Grid &g)
{
    return [&g](int x, int y) {
        g.plots++;
        if (x < 0 || y < 0 || x >= GRID_W || y >= GRID_H) {
            g.outside++;
            return;
        }
        if (g.px[y][x] == '#') {
            g.repeats++;
        }
        g.px[y][x] = '#';
    };
}

/* Compares the first `rows` grid rows (first `GRID_W` columns trimmed to strlen(golden[y])). */
static void expect_rows(const Grid *g, const char *const *golden, int rows)
{
    for (int y = 0; y < GRID_H; y++) {
        char want[GRID_W + 1];
        memset(want, '.', GRID_W);
        want[GRID_W] = '\0';
        if (y < rows) {
            memcpy(want, golden[y], strlen(golden[y]));
        }
        if (strcmp(want, g->px[y]) != 0) {
            MT_FAIL("row %d: expected %s, got %s", y, want, g->px[y]);
        }
    }
}

TEST(line_single_point)
{
    Grid g;
    grid_init(&g);
    raster_line(3, 4, 3, 4, grid_plot(g));
    ASSERT_EQ_INT(1, g.plots);
    ASSERT_EQ_CHAR('#', g.px[4][3]);
}

TEST(line_horizontal_and_vertical)
{
    Grid g;
    grid_init(&g);
    raster_line(1, 2, 6, 2, grid_plot(g));
    ASSERT_EQ_INT(6, g.plots);
    const char *h[] = {"", "", ".######"};
    expect_rows(&g, h, 3);

    grid_init(&g);
    raster_line(2, 1, 2, 4, grid_plot(g));
    ASSERT_EQ_INT(4, g.plots);
    const char *v[] = {"", "..#", "..#", "..#", "..#"};
    expect_rows(&g, v, 5);
}

TEST(line_diagonal)
{
    Grid g;
    grid_init(&g);
    raster_line(0, 0, 3, 3, grid_plot(g));
    const char *golden[] = {"#", ".#", "..#", "...#"};
    expect_rows(&g, golden, 4);
    ASSERT_EQ_INT(4, g.plots);
}

TEST(line_shallow_slope)
{
    Grid g;
    grid_init(&g);
    raster_line(0, 0, 5, 2, grid_plot(g));
    const char *golden[] = {"##", "..##", "....##"};
    expect_rows(&g, golden, 3);
    ASSERT_EQ_INT(6, g.plots);
}

TEST(line_steep_slope)
{
    Grid g;
    grid_init(&g);
    raster_line(0, 0, 2, 5, grid_plot(g));
    const char *golden[] = {"#", "#", ".#", ".#", "..#", "..#"};
    expect_rows(&g, golden, 6);
    ASSERT_EQ_INT(6, g.plots);
}

TEST(line_reversed_direction_covers_same_pixels)
{
    Grid a, b;
    grid_init(&a);
    grid_init(&b);
    raster_line(1, 1, 8, 1, grid_plot(a));
    raster_line(8, 1, 1, 1, grid_plot(b));
    ASSERT_EQ_INT(8, a.plots);
    ASSERT_EQ_INT(8, b.plots);
    ASSERT_EQ_INT(0, strcmp(a.px[1], b.px[1]));

    grid_init(&a);
    grid_init(&b);
    raster_line(0, 7, 5, 2, grid_plot(a));
    raster_line(5, 2, 0, 7, grid_plot(b));
    for (int y = 0; y < GRID_H; y++) {
        ASSERT_EQ_INT(0, strcmp(a.px[y], b.px[y]));
    }
}

TEST(line_reports_each_pixel_once_and_in_order_from_start)
{
    Grid g;
    grid_init(&g);
    raster_line(0, 0, 9, 4, grid_plot(g));
    ASSERT_EQ_INT(10, g.plots);
    ASSERT_EQ_INT(0, g.repeats);
    ASSERT_EQ_CHAR('#', g.px[0][0]);
    ASSERT_EQ_CHAR('#', g.px[4][9]);
}

TEST(line_may_leave_the_grid)
{
    Grid g;
    grid_init(&g);
    raster_line(-3, 0, 2, 0, grid_plot(g));
    ASSERT_EQ_INT(6, g.plots);
    ASSERT_EQ_INT(3, g.outside);
}

TEST(rect_outline)
{
    Grid g;
    grid_init(&g);
    raster_rect(1, 1, 5, 3, 0, grid_plot(g));
    const char *golden[] = {"", ".#####", ".#...#", ".#####"};
    expect_rows(&g, golden, 4);
    ASSERT_EQ_INT(12, g.plots);
    ASSERT_EQ_INT(0, g.repeats);
}

TEST(rect_filled)
{
    Grid g;
    grid_init(&g);
    raster_rect(1, 1, 4, 3, 1, grid_plot(g));
    const char *golden[] = {"", ".####", ".####", ".####"};
    expect_rows(&g, golden, 4);
    ASSERT_EQ_INT(12, g.plots);
    ASSERT_EQ_INT(0, g.repeats);
}

TEST(rect_corner_order_does_not_matter)
{
    Grid a, b;
    grid_init(&a);
    grid_init(&b);
    raster_rect(5, 3, 1, 1, 0, grid_plot(a));
    raster_rect(1, 1, 5, 3, 0, grid_plot(b));
    for (int y = 0; y < GRID_H; y++) {
        ASSERT_EQ_INT(0, strcmp(a.px[y], b.px[y]));
    }
    ASSERT_EQ_INT(b.plots, a.plots);
}

TEST(rect_degenerate_shapes)
{
    Grid g;
    grid_init(&g);
    raster_rect(2, 2, 2, 2, 0, grid_plot(g));
    ASSERT_EQ_INT(1, g.plots);

    grid_init(&g);
    raster_rect(1, 2, 4, 2, 0, grid_plot(g));
    ASSERT_EQ_INT(4, g.plots);
    ASSERT_EQ_INT(0, g.repeats);

    grid_init(&g);
    raster_rect(3, 1, 3, 4, 0, grid_plot(g));
    ASSERT_EQ_INT(4, g.plots);
    ASSERT_EQ_INT(0, g.repeats);

    grid_init(&g);
    raster_rect(1, 1, 2, 4, 0, grid_plot(g));
    ASSERT_EQ_INT(8, g.plots);
    ASSERT_EQ_INT(0, g.repeats);
}

TEST(ellipse_circle_outline)
{
    Grid g;
    grid_init(&g);
    raster_ellipse(0, 0, 6, 6, 0, grid_plot(g));
    const char *golden[] = {"..###", ".#...#", "#.....#", "#.....#", "#.....#", ".#...#", "..###"};
    expect_rows(&g, golden, 7);
}

TEST(ellipse_circle_filled)
{
    Grid g;
    grid_init(&g);
    raster_ellipse(0, 0, 6, 6, 1, grid_plot(g));
    const char *golden[] = {"..###", ".#####", "#######", "#######", "#######", ".#####", "..###"};
    expect_rows(&g, golden, 7);
}

TEST(ellipse_wide_and_flat)
{
    Grid g;
    grid_init(&g);
    raster_ellipse(0, 0, 8, 2, 0, grid_plot(g));
    /* Every column of the box is covered and the shape touches all three rows. */
    for (int x = 0; x <= 8; x++) {
        ASSERT_TRUE(g.px[0][x] == '#' || g.px[1][x] == '#' || g.px[2][x] == '#');
    }
    ASSERT_EQ_CHAR('#', g.px[0][4]);
    ASSERT_EQ_CHAR('#', g.px[2][4]);
    ASSERT_EQ_CHAR('#', g.px[1][0]);
    ASSERT_EQ_CHAR('#', g.px[1][8]);
    ASSERT_EQ_INT(0, g.outside);
}

TEST(ellipse_stays_inside_its_box_and_is_symmetric)
{
    for (int w = 1; w <= 11; w++) {
        for (int h = 1; h <= 11; h++) {
            for (int filled = 0; filled <= 1; filled++) {
                Grid g;
                grid_init(&g);
                raster_ellipse(0, 0, w - 1, h - 1, filled, grid_plot(g));
                int count = 0;
                for (int y = 0; y < GRID_H; y++) {
                    for (int x = 0; x < GRID_W; x++) {
                        int inside = x < w && y < h;
                        if (g.px[y][x] == '#') {
                            count++;
                            if (!inside) {
                                MT_FAIL("%dx%d: (%d, %d) outside box", w, h, x, y);
                                continue;
                            }
                            ASSERT_EQ_CHAR('#', g.px[y][w - 1 - x]);
                            ASSERT_EQ_CHAR('#', g.px[h - 1 - y][x]);
                        }
                    }
                }
                ASSERT_EQ_INT(0, g.outside);
                ASSERT_TRUE(count > 0);
            }
        }
    }
}

TEST(ellipse_filled_has_no_gaps_inside)
{
    for (int w = 1; w <= 11; w++) {
        for (int h = 1; h <= 11; h++) {
            Grid g;
            grid_init(&g);
            raster_ellipse(0, 0, w - 1, h - 1, 1, grid_plot(g));
            for (int y = 0; y < h; y++) {
                int first = -1, last = -1;
                for (int x = 0; x < w; x++) {
                    if (g.px[y][x] == '#') {
                        if (first < 0) {
                            first = x;
                        }
                        last = x;
                    }
                }
                if (first < 0) {
                    MT_FAIL("%dx%d: row %d is empty", w, h, y);
                    continue;
                }
                for (int x = first; x <= last; x++) {
                    ASSERT_EQ_CHAR('#', g.px[y][x]);
                }
            }
        }
    }
}

TEST(ellipse_corner_order_does_not_matter)
{
    Grid a, b;
    grid_init(&a);
    grid_init(&b);
    raster_ellipse(8, 5, 1, 1, 0, grid_plot(a));
    raster_ellipse(1, 1, 8, 5, 0, grid_plot(b));
    for (int y = 0; y < GRID_H; y++) {
        ASSERT_EQ_INT(0, strcmp(a.px[y], b.px[y]));
    }
}

TEST(ellipse_single_pixel)
{
    Grid g;
    grid_init(&g);
    raster_ellipse(4, 4, 4, 4, false, grid_plot(g));
    ASSERT_EQ_CHAR('#', g.px[4][4]);
    ASSERT_EQ_INT(0, g.outside);
}

/* Builds a cel from rows of characters: '.' transparent, 'r' red, 'b' blue. */
static Cel cel_from_rows(const char *const *rows, int w, int h)
{
    Cel c(w, h);
    Rgba red = {255, 0, 0, 255};
    Rgba blue = {0, 0, 255, 255};
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (rows[y][x] == 'r') {
                c.set(x, y, red);
            } else if (rows[y][x] == 'b') {
                c.set(x, y, blue);
            }
        }
    }
    return c;
}

TEST(flood_fill_whole_blank_cel)
{
    Cel c(4, 3);
    Grid g;
    grid_init(&g);
    ASSERT_TRUE(raster_flood_fill(c, 1, 1, grid_plot(g)));
    ASSERT_EQ_INT(12, g.plots);
    ASSERT_EQ_INT(0, g.repeats);
    const char *golden[] = {"####", "####", "####"};
    expect_rows(&g, golden, 3);
}

TEST(flood_fill_stops_at_other_colors)
{
    const char *rows[] = {
        "..r...",
        "..r...",
        "rrr...",
        "......",
    };
    Cel c = cel_from_rows(rows, 6, 4);
    Grid g;
    grid_init(&g);
    ASSERT_TRUE(raster_flood_fill(c, 0, 0, grid_plot(g)));
    const char *golden[] = {"##", "##", ""};
    expect_rows(&g, golden, 3);
    ASSERT_EQ_INT(4, g.plots);

    grid_init(&g);
    ASSERT_TRUE(raster_flood_fill(c, 5, 0, grid_plot(g)));
    /* 18 transparent pixels minus the 4 enclosed in the top-left corner. */
    ASSERT_EQ_INT(24 - 5 - 4, g.plots);
    ASSERT_EQ_CHAR('.', g.px[0][0]);
    ASSERT_EQ_CHAR('#', g.px[3][0]);
}

TEST(flood_fill_is_four_connected)
{
    const char *rows[] = {
        ".r.",
        "r..",
        "...",
    };
    Cel c = cel_from_rows(rows, 3, 3);
    Grid g;
    grid_init(&g);
    ASSERT_TRUE(raster_flood_fill(c, 0, 0, grid_plot(g)));
    ASSERT_EQ_INT(1, g.plots);
    ASSERT_EQ_CHAR('#', g.px[0][0]);
}

TEST(flood_fill_follows_winding_regions)
{
    const char *rows[] = {
        "......", "rrrrr.", "......", ".rrrrr", "......",
    };
    Cel c = cel_from_rows(rows, 6, 5);
    Grid g;
    grid_init(&g);
    ASSERT_TRUE(raster_flood_fill(c, 0, 0, grid_plot(g)));
    /* 30 pixels - 10 red = 20 blank, all one snake-shaped region. */
    ASSERT_EQ_INT(20, g.plots);
    ASSERT_EQ_INT(0, g.repeats);
    ASSERT_EQ_CHAR('#', g.px[4][0]);
}

TEST(flood_fill_selects_the_seed_color)
{
    const char *rows[] = {
        "rrb",
        "rrb",
    };
    Cel c = cel_from_rows(rows, 3, 2);
    Grid g;
    grid_init(&g);
    ASSERT_TRUE(raster_flood_fill(c, 1, 1, grid_plot(g)));
    const char *golden[] = {"##", "##"};
    expect_rows(&g, golden, 2);
    ASSERT_EQ_INT(4, g.plots);
}

TEST(flood_fill_rejects_out_of_bounds_seed)
{
    Cel c(3, 3);
    Grid g;
    grid_init(&g);
    ASSERT_TRUE(!raster_flood_fill(c, 3, 0, grid_plot(g)));
    ASSERT_TRUE(!raster_flood_fill(c, 0, -1, grid_plot(g)));
    ASSERT_EQ_INT(0, g.plots);
}

TEST(flood_fill_callback_may_edit_the_cel)
{
    Cel c(5, 5);
    ASSERT_TRUE(
        raster_flood_fill(c, 2, 2, [&c](int x, int y) { c.set(x, y, Rgba{0, 0, 255, 255}); }));
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            ASSERT_EQ_INT(255, c.get(x, y).b);
        }
    }
}

TEST(flood_fill_large_cel)
{
    Cel c(512, 512);
    Grid g;
    grid_init(&g);
    ASSERT_TRUE(raster_flood_fill(c, 0, 0, grid_plot(g)));
    ASSERT_EQ_INT(512 * 512, g.plots);
}

int main(void)
{
    RUN(line_single_point);
    RUN(line_horizontal_and_vertical);
    RUN(line_diagonal);
    RUN(line_shallow_slope);
    RUN(line_steep_slope);
    RUN(line_reversed_direction_covers_same_pixels);
    RUN(line_reports_each_pixel_once_and_in_order_from_start);
    RUN(line_may_leave_the_grid);
    RUN(rect_outline);
    RUN(rect_filled);
    RUN(rect_corner_order_does_not_matter);
    RUN(rect_degenerate_shapes);
    RUN(ellipse_circle_outline);
    RUN(ellipse_circle_filled);
    RUN(ellipse_wide_and_flat);
    RUN(ellipse_stays_inside_its_box_and_is_symmetric);
    RUN(ellipse_filled_has_no_gaps_inside);
    RUN(ellipse_corner_order_does_not_matter);
    RUN(ellipse_single_pixel);
    RUN(flood_fill_whole_blank_cel);
    RUN(flood_fill_stops_at_other_colors);
    RUN(flood_fill_is_four_connected);
    RUN(flood_fill_follows_winding_regions);
    RUN(flood_fill_selects_the_seed_color);
    RUN(flood_fill_rejects_out_of_bounds_seed);
    RUN(flood_fill_callback_may_edit_the_cel);
    RUN(flood_fill_large_cel);
    return TEST_SUMMARY();
}
