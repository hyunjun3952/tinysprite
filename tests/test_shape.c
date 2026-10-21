#include <string.h>

#include "minitest.h"
#include "tinysprite/raster.h"
#include "tinysprite/shape.h"

#define W 8
#define H 6

static const Rgba RED = {255, 0, 0, 255};

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

static void mark(int x, int y, void *user)
{
    char *grid = user;
    if (x >= 0 && y >= 0 && x < W && y < H) {
        grid[y * W + x] = 1;
    }
}

TEST(line_connects_the_corners)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ASSERT_EQ_INT(0, shape_stroke(h, c, SHAPE_LINE, 0, RED, 0, 0, 4, 4));
    const char *golden[H] = {"#.......", ".#......", "..#.....",
                             "...#....", "....#...", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(rectangle_outline_and_filled)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    shape_stroke(h, c, SHAPE_RECT, 0, RED, 1, 1, 4, 3);
    const char *outline[H] = {"........", ".####...", ".#..#...",
                              ".####...", "........", "........"};
    expect_rows(c, outline);
    shape_stroke(h, c, SHAPE_RECT, 1, RED, 1, 1, 4, 3);
    const char *filled[H] = {"........", ".####...", ".####...",
                             ".####...", "........", "........"};
    expect_rows(c, filled);
    cel_destroy(c);
    history_destroy(h);
}

TEST(corners_may_be_given_in_any_order)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    shape_stroke(h, c, SHAPE_RECT, 1, RED, 4, 3, 1, 1);
    const char *golden[H] = {"........", ".####...", ".####...",
                             ".####...", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(ellipse_matches_the_rasterizer)
{
    for (int filled = 0; filled <= 1; filled++) {
        History *h = history_create();
        Cel *c = cel_create(W, H);
        char want[W * H] = {0};
        raster_ellipse(1, 0, 6, 4, filled, mark, want);
        ASSERT_EQ_INT(0, shape_stroke(h, c, SHAPE_ELLIPSE, filled, RED, 1, 0, 6, 4));
        for (int i = 0; i < W * H; i++) {
            if ((cel_get(c, i % W, i / W).a != 0) != (want[i] != 0)) {
                MT_FAIL("filled=%d pixel %d differs", filled, i);
            }
        }
        cel_destroy(c);
        history_destroy(h);
    }
}

TEST(shape_is_clipped_to_the_cel)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ASSERT_EQ_INT(0, shape_stroke(h, c, SHAPE_RECT, 1, RED, 6, 4, 20, 20));
    const char *golden[H] = {"........", "........", "........",
                             "........", "......##", "......##"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(update_redraws_from_the_anchor)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    cel_set(c, 7, 5, RED); /* pre-existing pixel the preview must not destroy */
    ShapeStroke *s = shape_stroke_begin(h, c, SHAPE_RECT, 1, RED, 1, 1);
    ASSERT_TRUE(s != NULL);
    ASSERT_EQ_INT(0, shape_stroke_update(s, 7, 5));
    ASSERT_EQ_INT(0, shape_stroke_update(s, 3, 2));
    ASSERT_EQ_INT(0, shape_stroke_end(s));
    const char *golden[H] = {"........", ".###....", ".###....",
                             "........", "........", ".......#"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(begin_plots_the_anchor_pixel)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ShapeStroke *s = shape_stroke_begin(h, c, SHAPE_ELLIPSE, 0, RED, 2, 3);
    ASSERT_TRUE(s != NULL);
    ASSERT_EQ_INT(255, cel_get(c, 2, 3).a);
    ASSERT_EQ_INT(0, shape_stroke_end(s));
    cel_destroy(c);
    history_destroy(h);
}

TEST(whole_shape_is_one_undo_step)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ShapeStroke *s = shape_stroke_begin(h, c, SHAPE_RECT, 1, RED, 0, 0);
    shape_stroke_update(s, 3, 3);
    shape_stroke_update(s, 5, 2);
    shape_stroke_end(s);
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(0, cel_get(c, 0, 0).a);
    ASSERT_EQ_INT(0, cel_get(c, 5, 2).a);
    ASSERT_TRUE(!history_can_undo(h));
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(255, cel_get(c, 5, 2).a);
    cel_destroy(c);
    history_destroy(h);
}

TEST(cancel_restores_pixels_and_records_nothing)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ShapeStroke *s = shape_stroke_begin(h, c, SHAPE_RECT, 1, RED, 0, 0);
    shape_stroke_update(s, 4, 4);
    shape_stroke_cancel(s);
    ASSERT_EQ_INT(0, cel_get(c, 2, 2).a);
    ASSERT_TRUE(!history_can_undo(h));
    ASSERT_EQ_INT(0, history_begin_stroke(h, c)); /* history is free again */
    history_cancel_stroke(h);
    cel_destroy(c);
    history_destroy(h);
}

TEST(invalid_arguments_fail)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ASSERT_TRUE(shape_stroke_begin(NULL, c, SHAPE_LINE, 0, RED, 0, 0) == NULL);
    ASSERT_TRUE(shape_stroke_begin(h, NULL, SHAPE_LINE, 0, RED, 0, 0) == NULL);
    ShapeKind bad_kind;
    int raw_kind = 99;
    memcpy(&bad_kind, &raw_kind, sizeof(bad_kind));
    ASSERT_EQ_INT(-1, shape_stroke(h, c, bad_kind, 0, RED, 0, 0, 1, 1));
    ShapeStroke *s = shape_stroke_begin(h, c, SHAPE_LINE, 0, RED, 0, 0);
    ASSERT_TRUE(shape_stroke_begin(h, c, SHAPE_LINE, 0, RED, 1, 1) == NULL); /* one at a time */
    ASSERT_EQ_INT(-1, shape_stroke(h, c, SHAPE_LINE, 0, RED, 0, 0, 1, 1));
    ASSERT_EQ_INT(-1, shape_stroke_update(NULL, 1, 1));
    ASSERT_EQ_INT(-1, shape_stroke_end(NULL));
    shape_stroke_cancel(NULL);
    shape_stroke_cancel(s);
    cel_destroy(c);
    history_destroy(h);
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
    RUN(invalid_arguments_fail);
    return TEST_SUMMARY();
}
