#include <string.h>

#include "minitest.h"
#include "tinysprite/raster.h"
#include "tinysprite/tools.h"

#define W 8
#define H 6

static const Rgba RED = {255, 0, 0, 255};

static void expect_rows(const Cel &c, const char *const golden[H])
{
    for (int y = 0; y < H; y++) {
        char row[W + 1];
        for (int x = 0; x < W; x++) {
            row[x] = c.get(x, y).a ? '#' : '.';
        }
        row[W] = '\0';
        if (strcmp(golden[y], row) != 0) {
            MT_FAIL("row %d: expected %s, got %s", y, golden[y], row);
        }
    }
}

static RasterPlotFn mark(char *grid)
{
    return [grid](int x, int y) {
        if (x >= 0 && y >= 0 && x < W && y < H) {
            grid[y * W + x] = 1;
        }
    };
}

TEST(line_connects_the_corners)
{
    History h;
    Cel c(W, H);
    ASSERT_TRUE(shape_stroke(h, c, ShapeStyle{ShapeKind::Line, 0, RED, Symmetry{}}, ToolPoint{0, 0},
                             ToolPoint{4, 4}));
    const char *golden[H] = {"#.......", ".#......", "..#.....",
                             "...#....", "....#...", "........"};
    expect_rows(c, golden);
}

TEST(rectangle_outline_and_filled)
{
    History h;
    Cel c(W, H);
    shape_stroke(h, c, ShapeStyle{ShapeKind::Rect, 0, RED, Symmetry{}}, ToolPoint{1, 1},
                 ToolPoint{4, 3});
    const char *outline[H] = {"........", ".####...", ".#..#...",
                              ".####...", "........", "........"};
    expect_rows(c, outline);
    shape_stroke(h, c, ShapeStyle{ShapeKind::Rect, 1, RED, Symmetry{}}, ToolPoint{1, 1},
                 ToolPoint{4, 3});
    const char *filled[H] = {"........", ".####...", ".####...",
                             ".####...", "........", "........"};
    expect_rows(c, filled);
}

TEST(corners_may_be_given_in_any_order)
{
    History h;
    Cel c(W, H);
    shape_stroke(h, c, ShapeStyle{ShapeKind::Rect, 1, RED, Symmetry{}}, ToolPoint{4, 3},
                 ToolPoint{1, 1});
    const char *golden[H] = {"........", ".####...", ".####...",
                             ".####...", "........", "........"};
    expect_rows(c, golden);
}

TEST(ellipse_matches_the_rasterizer)
{
    for (int filled = 0; filled <= 1; filled++) {
        History h;
        Cel c(W, H);
        char want[W * H] = {0};
        raster_ellipse(1, 0, 6, 4, filled, mark(want));
        ASSERT_TRUE(shape_stroke(h, c, ShapeStyle{ShapeKind::Ellipse, filled != 0, RED, Symmetry{}},
                                 ToolPoint{1, 0}, ToolPoint{6, 4}));
        for (int i = 0; i < W * H; i++) {
            if ((c.get(i % W, i / W).a != 0) != (want[i] != 0)) {
                MT_FAIL("filled=%d pixel %d differs", filled, i);
            }
        }
    }
}

TEST(shape_is_clipped_to_the_cel)
{
    History h;
    Cel c(W, H);
    ASSERT_TRUE(shape_stroke(h, c, ShapeStyle{ShapeKind::Rect, 1, RED, Symmetry{}}, ToolPoint{6, 4},
                             ToolPoint{20, 20}));
    const char *golden[H] = {"........", "........", "........",
                             "........", "......##", "......##"};
    expect_rows(c, golden);
}

TEST(update_redraws_from_the_anchor)
{
    History h;
    Cel c(W, H);
    c.set(7, 5, RED); /* pre-existing pixel the preview must not destroy */
    auto s =
        ShapeStroke::begin(h, c, ShapeStyle{ShapeKind::Rect, 1, RED, Symmetry{}}, ToolPoint{1, 1});
    s->update(7, 5);
    s->update(3, 2);
    ASSERT_TRUE(s->end());
    const char *golden[H] = {"........", ".###....", ".###....",
                             "........", "........", ".......#"};
    expect_rows(c, golden);
}

TEST(begin_plots_the_anchor_pixel)
{
    History h;
    Cel c(W, H);
    auto s = ShapeStroke::begin(h, c, ShapeStyle{ShapeKind::Ellipse, 0, RED, Symmetry{}},
                                ToolPoint{2, 3});
    ASSERT_EQ_INT(255, c.get(2, 3).a);
    ASSERT_TRUE(s->end());
}

TEST(whole_shape_is_one_undo_step)
{
    History h;
    Cel c(W, H);
    auto s =
        ShapeStroke::begin(h, c, ShapeStyle{ShapeKind::Rect, 1, RED, Symmetry{}}, ToolPoint{0, 0});
    s->update(3, 3);
    s->update(5, 2);
    s->end();
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, c.get(0, 0).a);
    ASSERT_EQ_INT(0, c.get(5, 2).a);
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(255, c.get(5, 2).a);
}

TEST(cancel_restores_pixels_and_records_nothing)
{
    History h;
    Cel c(W, H);
    auto s =
        ShapeStroke::begin(h, c, ShapeStyle{ShapeKind::Rect, 1, RED, Symmetry{}}, ToolPoint{0, 0});
    s->update(4, 4);
    s->cancel();
    ASSERT_EQ_INT(0, c.get(2, 2).a);
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.beginStroke(c)); /* history is free again */
    h.cancelStroke();
}

TEST(only_one_stroke_at_a_time)
{
    History h;
    Cel c(W, H);
    auto s =
        ShapeStroke::begin(h, c, ShapeStyle{ShapeKind::Line, 0, RED, Symmetry{}}, ToolPoint{0, 0});
    ASSERT_TRUE(ShapeStroke::begin(h, c, ShapeStyle{ShapeKind::Line, 0, RED, Symmetry{}},
                                   ToolPoint{1, 1}) == nullptr);
    ASSERT_TRUE(!shape_stroke(h, c, ShapeStyle{ShapeKind::Line, 0, RED, Symmetry{}},
                              ToolPoint{0, 0}, ToolPoint{1, 1}));
    s->cancel();
}

TEST(mirror_x_reflects_a_line)
{
    History h;
    Cel c(W, H);
    Symmetry sym = Symmetry{1};
    ASSERT_TRUE(shape_stroke(h, c, ShapeStyle{ShapeKind::Line, 0, RED, sym}, ToolPoint{0, 0},
                             ToolPoint{2, 2}));
    const char *golden[H] = {"#......#", ".#....#.", "..#..#..",
                             "........", "........", "........"};
    expect_rows(c, golden);
    ASSERT_TRUE(h.undo());
    const char *blank[H] = {"........", "........", "........", "........", "........", "........"};
    expect_rows(c, blank);
}

TEST(mirror_both_reflects_a_filled_rect_into_four_corners)
{
    History h;
    Cel c(W, H);
    Symmetry sym = Symmetry{1, 1};
    ASSERT_TRUE(shape_stroke(h, c, ShapeStyle{ShapeKind::Rect, 1, RED, sym}, ToolPoint{0, 0},
                             ToolPoint{1, 1}));
    const char *golden[H] = {"##....##", "##....##", "........",
                             "........", "##....##", "##....##"};
    expect_rows(c, golden);
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(!h.canUndo());
}

TEST(live_mirrored_shape_redraws_both_halves)
{
    History h;
    Cel c(W, H);
    Symmetry sym = Symmetry{1};
    auto s = ShapeStroke::begin(h, c, ShapeStyle{ShapeKind::Line, 0, RED, sym}, ToolPoint{0, 0});
    s->update(3, 0);
    s->update(1, 0);
    const char *golden[H] = {"##....##", "........", "........",
                             "........", "........", "........"};
    expect_rows(c, golden);
    s->end();
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, c.get(7, 0).a);
}

TEST(zero_symmetry_matches_the_plain_api)
{
    History h;
    Cel c(W, H);
    ASSERT_TRUE(shape_stroke(h, c, ShapeStyle{ShapeKind::Line, 0, RED, Symmetry{}}, ToolPoint{0, 0},
                             ToolPoint{4, 4}));
    const char *golden[H] = {"#.......", ".#......", "..#.....",
                             "...#....", "....#...", "........"};
    expect_rows(c, golden);
}

int main(void)
{
    RUN(line_connects_the_corners);
    RUN(rectangle_outline_and_filled);
    RUN(corners_may_be_given_in_any_order);
    RUN(ellipse_matches_the_rasterizer);
    RUN(shape_is_clipped_to_the_cel);
    RUN(update_redraws_from_the_anchor);
    RUN(begin_plots_the_anchor_pixel);
    RUN(whole_shape_is_one_undo_step);
    RUN(cancel_restores_pixels_and_records_nothing);
    RUN(only_one_stroke_at_a_time);
    RUN(mirror_x_reflects_a_line);
    RUN(mirror_both_reflects_a_filled_rect_into_four_corners);
    RUN(live_mirrored_shape_redraws_both_halves);
    RUN(zero_symmetry_matches_the_plain_api);
    return TEST_SUMMARY();
}
