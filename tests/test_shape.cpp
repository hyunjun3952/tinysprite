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

TEST(diamond_outline_and_filled)
{
    History h;
    Cel c(W, H);
    shape_stroke(h, c, ShapeStyle{ShapeKind::Diamond, 0, RED, Symmetry{}}, ToolPoint{0, 0},
                 ToolPoint{4, 4});
    const char *outline[H] = {"..#.....", ".#.#....", "#...#...",
                              ".#.#....", "..#.....", "........"};
    expect_rows(c, outline);
    shape_stroke(h, c, ShapeStyle{ShapeKind::Diamond, 1, RED, Symmetry{}}, ToolPoint{0, 0},
                 ToolPoint{4, 4});
    const char *filled[H] = {"..#.....", ".###....", "#####...",
                             ".###....", "..#.....", "........"};
    expect_rows(c, filled);
}

TEST(every_shape_kind_draws_something)
{
    for (int k = 0; k < SHAPE_COUNT; k++) {
        for (int filled = 0; filled < 2; filled++) {
            History h;
            Cel c(16, 16);
            ASSERT_TRUE(shape_stroke(
                h, c, ShapeStyle{static_cast<ShapeKind>(k), filled != 0, RED, Symmetry{}},
                ToolPoint{2, 2}, ToolPoint{12, 12}));
            int lit = 0;
            for (int i = 0; i < 16 * 16; i++) {
                lit += c.pixels()[i].a != 0;
            }
            ASSERT_TRUE(lit > 0);
        }
    }
}

TEST(an_out_of_range_kind_draws_nothing_and_has_no_fill)
{
    History h;
    Cel c(8, 8);
    ShapeStyle style{ShapeKind::Count, true, RED, Symmetry{}};
    shape_stroke(h, c, style, ToolPoint{1, 1}, ToolPoint{6, 6});
    for (int i = 0; i < 8 * 8; i++) {
        ASSERT_TRUE(c.pixels()[i].a == 0);
    }
    ASSERT_TRUE(!shape_has_fill(ShapeKind::Count));
}

TEST(only_a_line_has_no_fill)
{
    for (int k = 0; k < SHAPE_COUNT; k++) {
        ShapeKind kind = static_cast<ShapeKind>(k);
        ASSERT_TRUE(shape_has_fill(kind) == (kind != ShapeKind::Line));
    }
}

TEST(polygon_shapes_stay_in_their_box_and_are_symmetric)
{
    const ShapeKind kinds[] = {ShapeKind::Triangle, ShapeKind::Diamond, ShapeKind::Hexagon,
                               ShapeKind::Star, ShapeKind::Cross};
    for (ShapeKind kind : kinds) {
        for (int filled = 0; filled < 2; filled++) {
            for (int size = 4; size <= 12; size += 2) { /* even offsets: an odd pixel width */
                History h;
                Cel c(16, 16);
                shape_stroke(h, c, ShapeStyle{kind, filled != 0, RED, Symmetry{}}, ToolPoint{2, 3},
                             ToolPoint{2 + size, 3 + size});
                bool any = false;
                for (int y = 0; y < 16; y++) {
                    for (int x = 0; x < 16; x++) {
                        bool on = c.get(x, y).a != 0;
                        any = any || on;
                        ASSERT_TRUE(!on || (x >= 2 && x <= 2 + size && y >= 3 && y <= 3 + size));
                        /* mirrored across the box's vertical axis */
                        ASSERT_TRUE(on == (c.get(4 + size - x, y).a != 0));
                    }
                }
                ASSERT_TRUE(any);
            }
        }
    }
}

TEST(filled_polygon_covers_its_outline_and_more)
{
    History h;
    Cel outline(16, 16);
    Cel filled(16, 16);
    shape_stroke(h, outline, ShapeStyle{ShapeKind::Star, 0, RED, Symmetry{}}, ToolPoint{0, 0},
                 ToolPoint{12, 12});
    shape_stroke(h, filled, ShapeStyle{ShapeKind::Star, 1, RED, Symmetry{}}, ToolPoint{0, 0},
                 ToolPoint{12, 12});
    int extra = 0;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            ASSERT_TRUE(outline.get(x, y).a == 0 || filled.get(x, y).a != 0);
            extra += outline.get(x, y).a == 0 && filled.get(x, y).a != 0;
        }
    }
    ASSERT_TRUE(extra > 0);
    /* the star is concave: the notch between two arms stays empty */
    ASSERT_TRUE(filled.get(0, 0).a == 0 && filled.get(12, 0).a == 0);
    ASSERT_TRUE(filled.get(6, 0).a != 0 && filled.get(6, 12).a == 0);
}

TEST(shapes_work_when_dragged_up_and_left)
{
    History h;
    Cel a(W, H);
    Cel b(W, H);
    ShapeStyle style{ShapeKind::Hexagon, 1, RED, Symmetry{}};
    shape_stroke(h, a, style, ToolPoint{0, 0}, ToolPoint{6, 4});
    shape_stroke(h, b, style, ToolPoint{6, 4}, ToolPoint{0, 0});
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            ASSERT_TRUE((a.get(x, y).a != 0) == (b.get(x, y).a != 0));
        }
    }
}

TEST(constrain_snaps_lines_to_eight_directions)
{
    ToolPoint a = {5, 5};
    ToolPoint h = shape_constrain(ShapeKind::Line, a, ToolPoint{12, 6});
    ASSERT_TRUE(h.x == 12 && h.y == 5);
    ToolPoint v = shape_constrain(ShapeKind::Line, a, ToolPoint{4, -3});
    ASSERT_TRUE(v.x == 5 && v.y == -3);
    ToolPoint d = shape_constrain(ShapeKind::Line, a, ToolPoint{9, 11});
    ASSERT_TRUE(d.x == 11 && d.y == 11); /* the longer side wins */
    ToolPoint u = shape_constrain(ShapeKind::Line, a, ToolPoint{1, 2});
    ASSERT_TRUE(u.x == 1 && u.y == 1); /* up-left: (-4, -3) is closer to the diagonal, side 4 */
}

TEST(constrain_makes_boxes_square)
{
    ToolPoint a = {2, 2};
    for (ShapeKind kind : {ShapeKind::Rect, ShapeKind::Ellipse, ShapeKind::Star}) {
        ToolPoint p = shape_constrain(kind, a, ToolPoint{8, 4});
        ASSERT_TRUE(p.x == 8 && p.y == 8);
        ToolPoint q = shape_constrain(kind, a, ToolPoint{-1, 0});
        ASSERT_TRUE(q.x == -1 && q.y == -1);
    }
    ToolPoint same = shape_constrain(ShapeKind::Rect, a, a);
    ASSERT_TRUE(same.x == 2 && same.y == 2);
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
    RUN(diamond_outline_and_filled);
    RUN(every_shape_kind_draws_something);
    RUN(an_out_of_range_kind_draws_nothing_and_has_no_fill);
    RUN(only_a_line_has_no_fill);
    RUN(polygon_shapes_stay_in_their_box_and_are_symmetric);
    RUN(filled_polygon_covers_its_outline_and_more);
    RUN(shapes_work_when_dragged_up_and_left);
    RUN(constrain_snaps_lines_to_eight_directions);
    RUN(constrain_makes_boxes_square);
    return TEST_SUMMARY();
}
