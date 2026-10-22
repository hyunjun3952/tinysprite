#include <cstring>

#include "minitest.h"
#include "tinysprite/tools.h"

#define W 8
#define H 5

static const Rgba RED = {255, 0, 0, 255};

/* Renders the cel as rows of '#' (opaque) and '.' (transparent). */
static void render(const Cel &c, char out[H][W + 1])
{
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            out[y][x] = c.get(x, y).a != 0 ? '#' : '.';
        }
        out[y][W] = '\0';
    }
}

static void expect_rows(const Cel &c, const char *const golden[H])
{
    char got[H][W + 1];
    render(c, got);
    for (int y = 0; y < H; y++) {
        if (strcmp(golden[y], got[y]) != 0) {
            MT_FAIL("row %d: expected %s, got %s", y, golden[y], got[y]);
        }
    }
}

static const char *const BLANK[H] = {"........", "........", "........", "........", "........"};

TEST(single_point_plots_one_pixel)
{
    History h;
    Cel c(W, H);
    ToolPoint p[] = {{3, 2}};
    ASSERT_TRUE(pencil_stroke(h, c, std::vector<ToolPoint>(p, p + 1), RED));
    const char *golden[H] = {"........", "........", "...#....", "........", "........"};
    expect_rows(c, golden);
    ASSERT_EQ_INT(255, c.get(3, 2).r);
}

TEST(points_are_joined_by_lines)
{
    History h;
    Cel c(W, H);
    ToolPoint p[] = {{0, 0}, {3, 0}, {3, 3}};
    ASSERT_TRUE(pencil_stroke(h, c, std::vector<ToolPoint>(p, p + 3), RED));
    const char *golden[H] = {"####....", "...#....", "...#....", "...#....", "........"};
    expect_rows(c, golden);
}

TEST(whole_stroke_is_one_undo_step)
{
    History h;
    Cel c(W, H);
    ToolPoint p[] = {{0, 0}, {7, 4}};
    pencil_stroke(h, c, std::vector<ToolPoint>(p, p + 2), RED);
    ASSERT_TRUE(h.undo());
    expect_rows(c, BLANK);
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(255, c.get(7, 4).a);
}

TEST(pixels_outside_the_cel_are_clipped)
{
    History h;
    Cel c(W, H);
    ToolPoint p[] = {{-3, 1}, {2, 1}};
    ASSERT_TRUE(pencil_stroke(h, c, std::vector<ToolPoint>(p, p + 2), RED));
    const char *golden[H] = {"........", "###.....", "........", "........", "........"};
    expect_rows(c, golden);

    ToolPoint far[] = {{20, 20}};
    ASSERT_TRUE(pencil_stroke(h, c, std::vector<ToolPoint>(far, far + 1), RED));
    expect_rows(c, golden);
}

TEST(eraser_clears_pixels_and_undo_restores_them)
{
    History h;
    Cel c(W, H);
    ToolPoint line[] = {{0, 2}, {7, 2}};
    pencil_stroke(h, c, std::vector<ToolPoint>(line, line + 2), RED);
    ToolPoint hole[] = {{2, 2}, {4, 2}};
    ASSERT_TRUE(pencil_stroke(h, c, std::vector<ToolPoint>(hole, hole + 2), Rgba{}));
    const char *golden[H] = {"........", "........", "##...###", "........", "........"};
    expect_rows(c, golden);
    ASSERT_EQ_INT(0, c.get(3, 2).r);

    h.undo();
    ASSERT_EQ_INT(255, c.get(3, 2).a);
    ASSERT_EQ_INT(255, c.get(3, 2).r);
}

TEST(erasing_blank_pixels_leaves_no_history)
{
    History h;
    Cel c(W, H);
    ToolPoint p[] = {{1, 1}, {5, 3}};
    ASSERT_TRUE(pencil_stroke(h, c, std::vector<ToolPoint>(p, p + 2), Rgba{}));
    ASSERT_TRUE(!h.canUndo());
}

TEST(retracing_a_pixel_still_undoes_cleanly)
{
    History h;
    Cel c(W, H);
    ToolPoint p[] = {{0, 0}, {4, 0}, {0, 0}};
    pencil_stroke(h, c, std::vector<ToolPoint>(p, p + 3), RED);
    h.undo();
    expect_rows(c, BLANK);
}

TEST(invalid_arguments_are_rejected)
{
    History h;
    Cel c(W, H);
    ToolPoint p[] = {{0, 0}};
    ASSERT_TRUE(!pencil_stroke(h, c, std::vector<ToolPoint>(p, p + 0), RED));
    expect_rows(c, BLANK);
    ASSERT_TRUE(!h.canUndo());
}

TEST(fails_while_another_stroke_is_active)
{
    History h;
    Cel c(W, H);
    h.beginStroke(c);
    ToolPoint p[] = {{0, 0}};
    ASSERT_TRUE(!pencil_stroke(h, c, std::vector<ToolPoint>(p, p + 1), RED));
    ASSERT_TRUE(h.endStroke());
}

TEST(live_stroke_joins_extended_points)
{
    History h;
    Cel c(W, H);
    auto s = PencilStroke::begin(h, c, RED, PencilOptions{}, 1, 1);
    s->extend(5, 1);
    s->extend(5, 3);
    ASSERT_TRUE(s->end());
    const char *const golden[H] = {"........", ".#####..", ".....#..", ".....#..", "........"};
    expect_rows(c, golden);
    ASSERT_TRUE(h.undo());
    expect_rows(c, BLANK);
    ASSERT_TRUE(!h.canUndo());
}

TEST(live_stroke_without_extend_plots_start_pixel)
{
    History h;
    Cel c(W, H);
    auto s = PencilStroke::begin(h, c, RED, PencilOptions{}, 2, 2);
    ASSERT_TRUE(s->end());
    ASSERT_EQ_INT(1, c.get(2, 2).a ? 1 : 0);
}

TEST(live_stroke_erase_clears_pixels)
{
    History h;
    Cel c(W, H);
    ToolPoint p[] = {{0, 0}, {3, 0}};
    ASSERT_TRUE(pencil_stroke(h, c, std::vector<ToolPoint>(p, p + 2), RED));
    auto s = PencilStroke::begin(h, c, Rgba{}, PencilOptions{}, 1, 0);
    s->extend(2, 0);
    ASSERT_TRUE(s->end());
    const char *const golden[H] = {"#..#....", "........", "........", "........", "........"};
    expect_rows(c, golden);
}

TEST(live_stroke_clips_out_of_bounds_and_skips_repeats)
{
    History h;
    Cel c(W, H);
    auto s = PencilStroke::begin(h, c, RED, PencilOptions{}, 6, 0);
    s->extend(6, 0);
    s->extend(10, 0);
    ASSERT_TRUE(s->end());
    const char *const golden[H] = {"......##", "........", "........", "........", "........"};
    expect_rows(c, golden);
}

TEST(live_stroke_fails_while_another_stroke_is_active)
{
    History h;
    Cel c(W, H);
    ASSERT_TRUE(h.beginStroke(c));
    ASSERT_TRUE(PencilStroke::begin(h, c, RED, PencilOptions{}, 0, 0) == nullptr);
    ASSERT_TRUE(h.endStroke());
}

TEST(live_stroke_cancel_rolls_back)
{
    History h;
    Cel c(W, H);
    auto s = PencilStroke::begin(h, c, RED, PencilOptions{}, 1, 1);
    s->extend(4, 1);
    s->cancel();
    expect_rows(c, BLANK);
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.beginStroke(c));
    ASSERT_TRUE(h.endStroke());
}

int main()
{
    RUN(single_point_plots_one_pixel);
    RUN(points_are_joined_by_lines);
    RUN(whole_stroke_is_one_undo_step);
    RUN(pixels_outside_the_cel_are_clipped);
    RUN(eraser_clears_pixels_and_undo_restores_them);
    RUN(erasing_blank_pixels_leaves_no_history);
    RUN(retracing_a_pixel_still_undoes_cleanly);
    RUN(invalid_arguments_are_rejected);
    RUN(fails_while_another_stroke_is_active);
    RUN(live_stroke_joins_extended_points);
    RUN(live_stroke_without_extend_plots_start_pixel);
    RUN(live_stroke_erase_clears_pixels);
    RUN(live_stroke_clips_out_of_bounds_and_skips_repeats);
    RUN(live_stroke_fails_while_another_stroke_is_active);
    RUN(live_stroke_cancel_rolls_back);
    return TEST_SUMMARY();
}
