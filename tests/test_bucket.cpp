#include <cstring>

#include "minitest.h"
#include "tinysprite/tools.h"

#define W 6
#define H 4

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

/* Renders the cel as rows: '.' transparent, 'r' red, 'b' blue, '#' anything else. */
static void expect_rows(const Cel &c, const char *const golden[H])
{
    for (int y = 0; y < H; y++) {
        char row[W + 1];
        for (int x = 0; x < W; x++) {
            Rgba p = c.get(x, y);
            char glyph = '#';
            if (p.a == 0) {
                glyph = '.';
            } else if (p.r == 255 && p.b == 0) {
                glyph = 'r';
            } else if (p.b == 255) {
                glyph = 'b';
            }
            row[x] = glyph;
        }
        row[W] = '\0';
        if (strcmp(golden[y], row) != 0) {
            MT_FAIL("row %d: expected %s, got %s", y, golden[y], row);
        }
    }
}

/* Vertical wall of red at x == 2 splits the cel. */
static void build_wall(History &h, Cel &c)
{
    ToolPoint wall[] = {{2, 0}, {2, 3}};
    pencil_stroke(h, c, std::vector<ToolPoint>(wall, wall + 2), RED);
}

TEST(fills_the_connected_region_only)
{
    History h;
    Cel c(W, H);
    build_wall(h, c);
    ASSERT_TRUE(bucket_fill(h, c, 0, 0, BLUE, Symmetry{}));
    const char *golden[H] = {"bbr...", "bbr...", "bbr...", "bbr..."};
    expect_rows(c, golden);
}

TEST(fill_is_one_undo_step)
{
    History h;
    Cel c(W, H);
    build_wall(h, c);
    bucket_fill(h, c, 4, 1, BLUE, Symmetry{});
    ASSERT_TRUE(h.undo());
    const char *after_undo[H] = {"..r...", "..r...", "..r...", "..r..."};
    expect_rows(c, after_undo);
    ASSERT_TRUE(h.redo());
    const char *after_redo[H] = {"..rbbb", "..rbbb", "..rbbb", "..rbbb"};
    expect_rows(c, after_redo);
}

TEST(filling_with_the_same_color_records_nothing)
{
    History h;
    Cel c(W, H);
    ASSERT_TRUE(bucket_fill(h, c, 0, 0, Rgba{0, 0, 0, 0}, Symmetry{}));
    ASSERT_TRUE(!h.canUndo());
}

TEST(fill_replaces_with_transparent_too)
{
    History h;
    Cel c(W, H);
    build_wall(h, c);
    bucket_fill(h, c, 2, 2, Rgba{0, 0, 0, 0}, Symmetry{});
    const char *golden[H] = {"......", "......", "......", "......"};
    expect_rows(c, golden);
}

TEST(invalid_arguments_and_active_stroke_fail)
{
    History h;
    Cel c(W, H);
    ASSERT_TRUE(!bucket_fill(h, c, -1, 0, BLUE, Symmetry{}));
    ASSERT_TRUE(!bucket_fill(h, c, 0, H, BLUE, Symmetry{}));
    h.beginStroke(c);
    ASSERT_TRUE(!bucket_fill(h, c, 0, 0, BLUE, Symmetry{}));
    h.cancelStroke();
    ASSERT_TRUE(!h.canUndo());
}

TEST(mirror_x_fills_the_reflected_region_too)
{
    History h;
    Cel c(W, H);
    /* A blue wall splits the cel into a left and a right region. */
    for (int y = 0; y < H; y++) {
        c.set(2, y, BLUE);
        c.set(3, y, BLUE);
    }
    auto sym = Symmetry{true};
    ASSERT_TRUE(bucket_fill(h, c, 0, 0, RED, sym));
    const char *golden[H] = {"rrbbrr", "rrbbrr", "rrbbrr", "rrbbrr"};
    expect_rows(c, golden);
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, c.get(0, 0).a);
    ASSERT_EQ_INT(0, c.get(5, 0).a);
}

TEST(mirror_seed_on_the_axis_fills_once)
{
    History h;
    Cel c(5, 4);
    auto sym = Symmetry{true, true};
    ASSERT_TRUE(bucket_fill(h, c, 2, 1, RED, sym));
    ASSERT_EQ_INT(255, c.get(0, 0).a);
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, c.get(0, 0).a);
    ASSERT_TRUE(!h.canUndo());
}

TEST(mirror_x_with_four_corners_fills_four_regions)
{
    History h;
    Cel c(W, H);
    for (int x = 0; x < W; x++) {
        c.set(x, 1, BLUE);
        c.set(x, 2, BLUE);
    }
    for (int y = 0; y < H; y++) {
        c.set(2, y, BLUE);
        c.set(3, y, BLUE);
    }
    auto sym = Symmetry{true, true};
    ASSERT_TRUE(bucket_fill(h, c, 0, 0, RED, sym));
    ASSERT_EQ_INT(255, c.get(0, 0).r);
    ASSERT_EQ_INT(255, c.get(5, 0).r);
    ASSERT_EQ_INT(255, c.get(0, 3).r);
    ASSERT_EQ_INT(255, c.get(5, 3).r);
}

int main()
{
    RUN(fills_the_connected_region_only);
    RUN(fill_is_one_undo_step);
    RUN(filling_with_the_same_color_records_nothing);
    RUN(fill_replaces_with_transparent_too);
    RUN(invalid_arguments_and_active_stroke_fail);
    RUN(mirror_x_fills_the_reflected_region_too);
    RUN(mirror_seed_on_the_axis_fills_once);
    RUN(mirror_x_with_four_corners_fills_four_regions);
    return TEST_SUMMARY();
}
