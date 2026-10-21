#include <string.h>

#include "minitest.h"
#include "tinysprite/pencil.h"

#define W 8
#define H 5

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

static void expect_rows(const Cel *c, const char *const golden[H])
{
    for (int y = 0; y < H; y++) {
        char row[W + 1];
        for (int x = 0; x < W; x++) {
            row[x] = cel_get(c, x, y).a ? '#' : '.';
        }
        row[W] = '\0';
        if (strcmp(golden[y], row) != 0) {
            MT_FAIL("row %d: expected %s, got %s", y, golden[y], row);
        }
    }
}

static const char *const BLANK[H] = {"........", "........", "........", "........", "........"};

/* Draws the staircase (0,0) (1,0) (1,1) (2,1) (2,2): every second pixel is an L-shaped corner. */
static void draw_staircase(PencilStroke *s)
{
    pencil_stroke_extend(s, 1, 0);
    pencil_stroke_extend(s, 1, 1);
    pencil_stroke_extend(s, 2, 1);
    pencil_stroke_extend(s, 2, 2);
}

TEST(without_options_the_staircase_keeps_every_pixel)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, (PencilOptions){0}, 0, 0);
    ASSERT_TRUE(s != NULL);
    draw_staircase(s);
    pencil_stroke_end(s);
    const char *golden[H] = {"##......", ".##.....", "..#.....", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(pixel_perfect_removes_l_corners)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilOptions opt = {.pixel_perfect = 1};
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, opt, 0, 0);
    ASSERT_TRUE(s != NULL);
    draw_staircase(s);
    pencil_stroke_end(s);
    const char *golden[H] = {"#.......", ".#......", "..#.....", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(pixel_perfect_leaves_straight_lines_and_diagonals_alone)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilOptions opt = {.pixel_perfect = 1};
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, opt, 0, 0);
    pencil_stroke_extend(s, 7, 0);
    pencil_stroke_end(s);
    s = pencil_stroke_begin_ex(h, c, RED, 0, opt, 0, 1);
    pencil_stroke_extend(s, 3, 4);
    pencil_stroke_end(s);
    const char *golden[H] = {"########", "#.......", ".#......", "..#.....", "...#...."};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(pixel_perfect_cuts_the_corner_of_a_right_angle_turn)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilOptions opt = {.pixel_perfect = 1};
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, opt, 0, 0);
    pencil_stroke_extend(s, 3, 0);
    pencil_stroke_extend(s, 3, 3);
    pencil_stroke_end(s);
    const char *golden[H] = {"###.....", "...#....", "...#....", "...#....", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(pixel_perfect_restores_the_pixel_that_was_there_before)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    cel_set(c, 1, 0, BLUE); /* will be overdrawn, then un-drawn as a corner */
    PencilOptions opt = {.pixel_perfect = 1};
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, opt, 0, 0);
    pencil_stroke_extend(s, 1, 0);
    pencil_stroke_extend(s, 1, 1);
    pencil_stroke_end(s);
    ASSERT_EQ_INT(255, cel_get(c, 0, 0).r);
    ASSERT_EQ_INT(255, cel_get(c, 1, 1).r);
    ASSERT_EQ_INT(255, cel_get(c, 1, 0).b);
    ASSERT_EQ_INT(0, cel_get(c, 1, 0).r);
    cel_destroy(c);
    history_destroy(h);
}

TEST(pixel_perfect_stroke_undoes_in_one_step)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilOptions opt = {.pixel_perfect = 1};
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, opt, 0, 0);
    draw_staircase(s);
    pencil_stroke_end(s);
    ASSERT_EQ_INT(0, history_undo(h));
    expect_rows(c, BLANK);
    ASSERT_TRUE(!history_can_undo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(pixel_perfect_eraser_also_skips_corners)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            cel_set(c, i, j, RED);
        }
    }
    PencilOptions opt = {.pixel_perfect = 1};
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 1, opt, 0, 0);
    draw_staircase(s);
    pencil_stroke_end(s);
    ASSERT_EQ_INT(0, cel_get(c, 0, 0).a);
    ASSERT_EQ_INT(0, cel_get(c, 1, 1).a);
    ASSERT_EQ_INT(0, cel_get(c, 2, 2).a);
    ASSERT_EQ_INT(255, cel_get(c, 1, 0).a);
    ASSERT_EQ_INT(255, cel_get(c, 2, 1).a);
    cel_destroy(c);
    history_destroy(h);
}

TEST(mirror_x_reflects_across_the_vertical_axis)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilOptions opt = {.mirror_x = 1};
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, opt, 1, 1);
    pencil_stroke_extend(s, 2, 1);
    pencil_stroke_end(s);
    const char *golden[H] = {"........", ".##..##.", "........", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(mirror_y_reflects_across_the_horizontal_axis)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilOptions opt = {.mirror_y = 1};
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, opt, 3, 0);
    pencil_stroke_extend(s, 3, 1);
    pencil_stroke_end(s);
    const char *golden[H] = {"...#....", "...#....", "........", "...#....", "...#...."};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(mirror_both_axes_makes_four_copies_and_one_undo_step)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilOptions opt = {.mirror_x = 1, .mirror_y = 1};
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, opt, 1, 1);
    pencil_stroke_end(s);
    const char *golden[H] = {"........", ".#....#.", "........", ".#....#.", "........"};
    expect_rows(c, golden);
    ASSERT_EQ_INT(0, history_undo(h));
    expect_rows(c, BLANK);
    ASSERT_TRUE(!history_can_undo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(pixel_on_the_axis_is_not_drawn_twice)
{
    History *h = history_create();
    Cel *c = cel_create(7, 5); /* odd width: x == 3 is its own mirror */
    PencilOptions opt = {.mirror_x = 1};
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, opt, 3, 2);
    pencil_stroke_end(s);
    ASSERT_EQ_INT(255, cel_get(c, 3, 2).a);
    ASSERT_EQ_INT(0, cel_get(c, 2, 2).a);
    ASSERT_EQ_INT(0, cel_get(c, 4, 2).a);
    cel_destroy(c);
    history_destroy(h);
}

TEST(mirror_copy_outside_the_cel_is_clipped)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilOptions opt = {.mirror_x = 1};
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, opt, -2, 1); /* mirror lands on x == 9 */
    pencil_stroke_extend(s, 0, 1);
    pencil_stroke_end(s);
    const char *golden[H] = {"........", "#......#", "........", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(pixel_perfect_and_mirror_work_together)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilOptions opt = {.pixel_perfect = 1, .mirror_x = 1};
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, opt, 0, 0);
    draw_staircase(s);
    pencil_stroke_end(s);
    const char *golden[H] = {"#......#", ".#....#.", "..#..#..", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(begin_ex_rejects_bad_arguments_and_cancel_rolls_back)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilOptions opt = {.pixel_perfect = 1, .mirror_x = 1};
    ASSERT_TRUE(pencil_stroke_begin_ex(NULL, c, RED, 0, opt, 0, 0) == NULL);
    ASSERT_TRUE(pencil_stroke_begin_ex(h, NULL, RED, 0, opt, 0, 0) == NULL);
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, opt, 1, 1);
    ASSERT_TRUE(pencil_stroke_begin_ex(h, c, RED, 0, opt, 2, 2) == NULL);
    draw_staircase(s);
    pencil_stroke_cancel(s);
    expect_rows(c, BLANK);
    ASSERT_TRUE(!history_can_undo(h));
    cel_destroy(c);
    history_destroy(h);
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
