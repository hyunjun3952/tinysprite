#include <string.h>

#include "minitest.h"
#include "tinysprite/tools.h"

#define W 8
#define H 5

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

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

static const char *const BLANK[H] = {"........", "........", "........", "........", "........"};

/* Draws the staircase (0,0) (1,0) (1,1) (2,1) (2,2): every second pixel is an L-shaped corner. */
static void draw_staircase(PencilStroke &stroke)
{
    stroke.extend(1, 0);
    stroke.extend(1, 1);
    stroke.extend(2, 1);
    stroke.extend(2, 2);
}

TEST(without_options_the_staircase_keeps_every_pixel)
{
    History h;
    Cel c(W, H);
    auto s = PencilStroke::begin(h, c, RED, PencilOptions{}, 0, 0);
    draw_staircase(*s);
    s->end();
    const char *golden[H] = {"##......", ".##.....", "..#.....", "........", "........"};
    expect_rows(c, golden);
}

TEST(pixel_perfect_removes_l_corners)
{
    History h;
    Cel c(W, H);
    PencilOptions opt = PencilOptions{true};
    auto s = PencilStroke::begin(h, c, RED, opt, 0, 0);
    draw_staircase(*s);
    s->end();
    const char *golden[H] = {"#.......", ".#......", "..#.....", "........", "........"};
    expect_rows(c, golden);
}

TEST(pixel_perfect_leaves_straight_lines_and_diagonals_alone)
{
    History h;
    Cel c(W, H);
    PencilOptions opt = PencilOptions{true};
    auto s = PencilStroke::begin(h, c, RED, opt, 0, 0);
    s->extend(7, 0);
    s->end();
    s = PencilStroke::begin(h, c, RED, opt, 0, 1);
    s->extend(3, 4);
    s->end();
    const char *golden[H] = {"########", "#.......", ".#......", "..#.....", "...#...."};
    expect_rows(c, golden);
}

TEST(pixel_perfect_cuts_the_corner_of_a_right_angle_turn)
{
    History h;
    Cel c(W, H);
    PencilOptions opt = PencilOptions{true};
    auto s = PencilStroke::begin(h, c, RED, opt, 0, 0);
    s->extend(3, 0);
    s->extend(3, 3);
    s->end();
    const char *golden[H] = {"###.....", "...#....", "...#....", "...#....", "........"};
    expect_rows(c, golden);
}

TEST(pixel_perfect_restores_the_pixel_that_was_there_before)
{
    History h;
    Cel c(W, H);
    c.set(1, 0, BLUE); /* will be overdrawn, then un-drawn as a corner */
    PencilOptions opt = PencilOptions{true};
    auto s = PencilStroke::begin(h, c, RED, opt, 0, 0);
    s->extend(1, 0);
    s->extend(1, 1);
    s->end();
    ASSERT_EQ_INT(255, c.get(0, 0).r);
    ASSERT_EQ_INT(255, c.get(1, 1).r);
    ASSERT_EQ_INT(255, c.get(1, 0).b);
    ASSERT_EQ_INT(0, c.get(1, 0).r);
}

TEST(pixel_perfect_stroke_undoes_in_one_step)
{
    History h;
    Cel c(W, H);
    PencilOptions opt = PencilOptions{true};
    auto s = PencilStroke::begin(h, c, RED, opt, 0, 0);
    draw_staircase(*s);
    s->end();
    ASSERT_TRUE(h.undo());
    expect_rows(c, BLANK);
    ASSERT_TRUE(!h.canUndo());
}

TEST(pixel_perfect_eraser_also_skips_corners)
{
    History h;
    Cel c(W, H);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            c.set(i, j, RED);
        }
    }
    PencilOptions opt = PencilOptions{true};
    auto s = PencilStroke::begin(h, c, Rgba{}, opt, 0, 0);
    draw_staircase(*s);
    s->end();
    ASSERT_EQ_INT(0, c.get(0, 0).a);
    ASSERT_EQ_INT(0, c.get(1, 1).a);
    ASSERT_EQ_INT(0, c.get(2, 2).a);
    ASSERT_EQ_INT(255, c.get(1, 0).a);
    ASSERT_EQ_INT(255, c.get(2, 1).a);
}

TEST(mirror_x_reflects_across_the_vertical_axis)
{
    History h;
    Cel c(W, H);
    PencilOptions opt = PencilOptions{false, Symmetry{true}};
    auto s = PencilStroke::begin(h, c, RED, opt, 1, 1);
    s->extend(2, 1);
    s->end();
    const char *golden[H] = {"........", ".##..##.", "........", "........", "........"};
    expect_rows(c, golden);
}

TEST(mirror_y_reflects_across_the_horizontal_axis)
{
    History h;
    Cel c(W, H);
    PencilOptions opt = PencilOptions{false, Symmetry{false, true}};
    auto s = PencilStroke::begin(h, c, RED, opt, 3, 0);
    s->extend(3, 1);
    s->end();
    const char *golden[H] = {"...#....", "...#....", "........", "...#....", "...#...."};
    expect_rows(c, golden);
}

TEST(mirror_both_axes_makes_four_copies_and_one_undo_step)
{
    History h;
    Cel c(W, H);
    PencilOptions opt = PencilOptions{false, Symmetry{true, true}};
    auto s = PencilStroke::begin(h, c, RED, opt, 1, 1);
    s->end();
    const char *golden[H] = {"........", ".#....#.", "........", ".#....#.", "........"};
    expect_rows(c, golden);
    ASSERT_TRUE(h.undo());
    expect_rows(c, BLANK);
    ASSERT_TRUE(!h.canUndo());
}

TEST(pixel_on_the_axis_is_not_drawn_twice)
{
    History h;
    Cel c(7, 5); /* odd width: x == 3 is its own mirror */
    PencilOptions opt = PencilOptions{false, Symmetry{true}};
    auto s = PencilStroke::begin(h, c, RED, opt, 3, 2);
    s->end();
    ASSERT_EQ_INT(255, c.get(3, 2).a);
    ASSERT_EQ_INT(0, c.get(2, 2).a);
    ASSERT_EQ_INT(0, c.get(4, 2).a);
}

TEST(mirror_copy_outside_the_cel_is_clipped)
{
    History h;
    Cel c(W, H);
    PencilOptions opt = PencilOptions{false, Symmetry{true}};
    auto s = PencilStroke::begin(h, c, RED, opt, -2, 1); /* mirror lands on x == 9 */
    s->extend(0, 1);
    s->end();
    const char *golden[H] = {"........", "#......#", "........", "........", "........"};
    expect_rows(c, golden);
}

TEST(pixel_perfect_and_mirror_work_together)
{
    History h;
    Cel c(W, H);
    PencilOptions opt = PencilOptions{true, Symmetry{true}};
    auto s = PencilStroke::begin(h, c, RED, opt, 0, 0);
    draw_staircase(*s);
    s->end();
    const char *golden[H] = {"#......#", ".#....#.", "..#..#..", "........", "........"};
    expect_rows(c, golden);
}

TEST(begin_ex_rejects_bad_arguments_and_cancel_rolls_back)
{
    History h;
    Cel c(W, H);
    PencilOptions opt = PencilOptions{true, Symmetry{true}};
    auto s = PencilStroke::begin(h, c, RED, opt, 1, 1);
    ASSERT_TRUE(PencilStroke::begin(h, c, RED, opt, 2, 2) == nullptr);
    draw_staircase(*s);
    s->cancel();
    expect_rows(c, BLANK);
    ASSERT_TRUE(!h.canUndo());
}

int main(void)
{
    RUN(without_options_the_staircase_keeps_every_pixel);
    RUN(pixel_perfect_removes_l_corners);
    RUN(pixel_perfect_leaves_straight_lines_and_diagonals_alone);
    RUN(pixel_perfect_cuts_the_corner_of_a_right_angle_turn);
    RUN(pixel_perfect_restores_the_pixel_that_was_there_before);
    RUN(pixel_perfect_stroke_undoes_in_one_step);
    RUN(pixel_perfect_eraser_also_skips_corners);
    RUN(mirror_x_reflects_across_the_vertical_axis);
    RUN(mirror_y_reflects_across_the_horizontal_axis);
    RUN(mirror_both_axes_makes_four_copies_and_one_undo_step);
    RUN(pixel_on_the_axis_is_not_drawn_twice);
    RUN(mirror_copy_outside_the_cel_is_clipped);
    RUN(pixel_perfect_and_mirror_work_together);
    RUN(begin_ex_rejects_bad_arguments_and_cancel_rolls_back);
    return TEST_SUMMARY();
}
