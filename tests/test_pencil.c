#include <string.h>

#include "minitest.h"
#include "tinysprite/pencil.h"

#define W 8
#define H 5

static const Rgba RED = {255, 0, 0, 255};

/* Renders the cel as rows of '#' (opaque) and '.' (transparent). */
static void render(const Cel *c, char out[H][W + 1])
{
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            out[y][x] = cel_get(c, x, y).a ? '#' : '.';
        }
        out[y][W] = '\0';
    }
}

static void expect_rows(const Cel *c, const char *const golden[H])
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
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ToolPoint p[] = {{3, 2}};
    ASSERT_EQ_INT(0, pencil_stroke(h, c, p, 1, RED));
    const char *golden[H] = {"........", "........", "...#....", "........", "........"};
    expect_rows(c, golden);
    ASSERT_EQ_INT(255, cel_get(c, 3, 2).r);
    cel_destroy(c);
    history_destroy(h);
}

TEST(points_are_joined_by_lines)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ToolPoint p[] = {{0, 0}, {3, 0}, {3, 3}};
    ASSERT_EQ_INT(0, pencil_stroke(h, c, p, 3, RED));
    const char *golden[H] = {"####....", "...#....", "...#....", "...#....", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(whole_stroke_is_one_undo_step)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ToolPoint p[] = {{0, 0}, {7, 4}};
    pencil_stroke(h, c, p, 2, RED);
    ASSERT_EQ_INT(0, history_undo(h));
    expect_rows(c, BLANK);
    ASSERT_TRUE(!history_can_undo(h));
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(255, cel_get(c, 7, 4).a);
    cel_destroy(c);
    history_destroy(h);
}

TEST(pixels_outside_the_cel_are_clipped)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ToolPoint p[] = {{-3, 1}, {2, 1}};
    ASSERT_EQ_INT(0, pencil_stroke(h, c, p, 2, RED));
    const char *golden[H] = {"........", "###.....", "........", "........", "........"};
    expect_rows(c, golden);

    ToolPoint far[] = {{20, 20}};
    ASSERT_EQ_INT(0, pencil_stroke(h, c, far, 1, RED));
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(eraser_clears_pixels_and_undo_restores_them)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ToolPoint line[] = {{0, 2}, {7, 2}};
    pencil_stroke(h, c, line, 2, RED);
    ToolPoint hole[] = {{2, 2}, {4, 2}};
    ASSERT_EQ_INT(0, eraser_stroke(h, c, hole, 2));
    const char *golden[H] = {"........", "........", "##...###", "........", "........"};
    expect_rows(c, golden);
    ASSERT_EQ_INT(0, cel_get(c, 3, 2).r);

    history_undo(h);
    ASSERT_EQ_INT(255, cel_get(c, 3, 2).a);
    ASSERT_EQ_INT(255, cel_get(c, 3, 2).r);
    cel_destroy(c);
    history_destroy(h);
}

TEST(erasing_blank_pixels_leaves_no_history)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ToolPoint p[] = {{1, 1}, {5, 3}};
    ASSERT_EQ_INT(0, eraser_stroke(h, c, p, 2));
    ASSERT_TRUE(!history_can_undo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(retracing_a_pixel_still_undoes_cleanly)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ToolPoint p[] = {{0, 0}, {4, 0}, {0, 0}};
    pencil_stroke(h, c, p, 3, RED);
    history_undo(h);
    expect_rows(c, BLANK);
    cel_destroy(c);
    history_destroy(h);
}

TEST(invalid_arguments_are_rejected)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ToolPoint p[] = {{0, 0}};
    ASSERT_EQ_INT(-1, pencil_stroke(NULL, c, p, 1, RED));
    ASSERT_EQ_INT(-1, pencil_stroke(h, NULL, p, 1, RED));
    ASSERT_EQ_INT(-1, pencil_stroke(h, c, NULL, 1, RED));
    ASSERT_EQ_INT(-1, pencil_stroke(h, c, p, 0, RED));
    ASSERT_EQ_INT(-1, eraser_stroke(h, c, p, -1));
    expect_rows(c, BLANK);
    ASSERT_TRUE(!history_can_undo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(fails_while_another_stroke_is_active)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    history_begin_stroke(h, c);
    ToolPoint p[] = {{0, 0}};
    ASSERT_EQ_INT(-1, pencil_stroke(h, c, p, 1, RED));
    ASSERT_EQ_INT(0, history_end_stroke(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(live_stroke_joins_extended_points)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilStroke *s = pencil_stroke_begin(h, c, RED, (PencilOptions){0}, 1, 1);
    ASSERT_TRUE(s != NULL);
    ASSERT_EQ_INT(0, pencil_stroke_extend(s, 5, 1));
    ASSERT_EQ_INT(0, pencil_stroke_extend(s, 5, 3));
    ASSERT_EQ_INT(0, pencil_stroke_end(s));
    const char *const golden[H] = {"........", ".#####..", ".....#..", ".....#..", "........"};
    expect_rows(c, golden);
    ASSERT_EQ_INT(0, history_undo(h));
    expect_rows(c, BLANK);
    ASSERT_TRUE(!history_can_undo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(live_stroke_without_extend_plots_start_pixel)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilStroke *s = pencil_stroke_begin(h, c, RED, (PencilOptions){0}, 2, 2);
    ASSERT_TRUE(s != NULL);
    ASSERT_EQ_INT(0, pencil_stroke_end(s));
    ASSERT_EQ_INT(1, cel_get(c, 2, 2).a ? 1 : 0);
    cel_destroy(c);
    history_destroy(h);
}

TEST(live_stroke_erase_clears_pixels)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ToolPoint p[] = {{0, 0}, {3, 0}};
    ASSERT_EQ_INT(0, pencil_stroke(h, c, p, 2, RED));
    PencilStroke *s = pencil_stroke_begin(h, c, RGBA_TRANSPARENT, (PencilOptions){0}, 1, 0);
    ASSERT_TRUE(s != NULL);
    ASSERT_EQ_INT(0, pencil_stroke_extend(s, 2, 0));
    ASSERT_EQ_INT(0, pencil_stroke_end(s));
    const char *const golden[H] = {"#..#....", "........", "........", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(live_stroke_clips_out_of_bounds_and_skips_repeats)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilStroke *s = pencil_stroke_begin(h, c, RED, (PencilOptions){0}, 6, 0);
    ASSERT_TRUE(s != NULL);
    ASSERT_EQ_INT(0, pencil_stroke_extend(s, 6, 0));
    ASSERT_EQ_INT(0, pencil_stroke_extend(s, 10, 0));
    ASSERT_EQ_INT(0, pencil_stroke_end(s));
    const char *const golden[H] = {"......##", "........", "........", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(live_stroke_fails_while_another_stroke_is_active)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ASSERT_EQ_INT(0, history_begin_stroke(h, c));
    ASSERT_TRUE(pencil_stroke_begin(h, c, RED, (PencilOptions){0}, 0, 0) == NULL);
    ASSERT_TRUE(pencil_stroke_begin(NULL, c, RED, (PencilOptions){0}, 0, 0) == NULL);
    ASSERT_TRUE(pencil_stroke_begin(h, NULL, RED, (PencilOptions){0}, 0, 0) == NULL);
    ASSERT_EQ_INT(0, history_end_stroke(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(live_stroke_cancel_rolls_back)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilStroke *s = pencil_stroke_begin(h, c, RED, (PencilOptions){0}, 1, 1);
    ASSERT_TRUE(s != NULL);
    ASSERT_EQ_INT(0, pencil_stroke_extend(s, 4, 1));
    pencil_stroke_cancel(s);
    expect_rows(c, BLANK);
    ASSERT_TRUE(!history_can_undo(h));
    ASSERT_EQ_INT(0, history_begin_stroke(h, c));
    ASSERT_EQ_INT(0, history_end_stroke(h));
    cel_destroy(c);
    history_destroy(h);
}

int main(void)
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
