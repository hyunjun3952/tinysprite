#include <string.h>

#include "minitest.h"
#include "tinysprite/pencil.h"

#define W 8
#define H 5

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

/* A one-pixel click with the brush at (x, y). */
static Cel *dab(PencilOptions options, int x, int y)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, options, x, y);
    ASSERT_TRUE(s != NULL);
    pencil_stroke_end(s);
    history_destroy(h);
    return c;
}

TEST(size_zero_and_one_are_a_single_pixel)
{
    const char *golden[H] = {"........", "........", "...#....", "........", "........"};
    Cel *c = dab((PencilOptions){0}, 3, 2);
    expect_rows(c, golden);
    cel_destroy(c);
    c = dab((PencilOptions){.size = 1}, 3, 2);
    expect_rows(c, golden);
    cel_destroy(c);
}

TEST(even_square_brush_grows_right_and_down)
{
    Cel *c = dab((PencilOptions){.size = 2}, 3, 2);
    const char *golden[H] = {"........", "........", "...##...", "...##...", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
}

TEST(odd_square_brush_is_centered)
{
    Cel *c = dab((PencilOptions){.size = 3}, 3, 2);
    const char *golden[H] = {"........", "..###...", "..###...", "..###...", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
}

TEST(round_brush_rounds_the_corners)
{
    Cel *c = dab((PencilOptions){.size = 3, .round = 1}, 3, 2);
    const char *three[H] = {"........", "...#....", "..###...", "...#....", "........"};
    expect_rows(c, three);
    cel_destroy(c);

    c = dab((PencilOptions){.size = 4, .round = 1}, 3, 2);
    const char *four[H] = {"........", "...##...", "..####..", "..####..", "...##..."};
    expect_rows(c, four);
    cel_destroy(c);

    c = dab((PencilOptions){.size = 2, .round = 1}, 3, 2);
    const char *two[H] = {"........", "........", "...##...", "...##...", "........"};
    expect_rows(c, two);
    cel_destroy(c);
}

TEST(brush_is_clipped_at_the_cel_edge)
{
    Cel *c = dab((PencilOptions){.size = 3}, 0, 0);
    const char *golden[H] = {"##......", "##......", "........", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
}

TEST(dragging_sweeps_the_brush_along_the_line)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 0, (PencilOptions){.size = 2}, 1, 2);
    pencil_stroke_extend(s, 5, 2);
    pencil_stroke_end(s);
    const char *golden[H] = {"........", "........", ".######.", ".######.", "........"};
    expect_rows(c, golden);
    ASSERT_EQ_INT(0, history_undo(h));
    expect_rows(c, (const char *[H]){"........", "........", "........", "........", "........"});
    cel_destroy(c);
    history_destroy(h);
}

TEST(mirrored_brush_reflects_every_pixel_of_the_stamp)
{
    Cel *c = dab((PencilOptions){.size = 2, .mirror_x = 1}, 1, 1);
    const char *golden[H] = {"........", ".##..##.", ".##..##.", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);

    c = dab((PencilOptions){.size = 2, .mirror_y = 1}, 1, 0);
    const char *vertical[H] = {".##.....", ".##.....", "........", ".##.....", ".##....."};
    expect_rows(c, vertical);
    cel_destroy(c);
}

TEST(eraser_uses_the_brush_too)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            cel_set(c, x, y, RED);
        }
    }
    PencilStroke *s = pencil_stroke_begin_ex(h, c, RED, 1, (PencilOptions){.size = 3}, 3, 2);
    pencil_stroke_end(s);
    const char *golden[H] = {"########", "##...###", "##...###", "##...###", "########"};
    expect_rows(c, golden);
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(255, cel_get(c, 3, 2).a);
    cel_destroy(c);
    history_destroy(h);
}

TEST(pixel_perfect_is_ignored_for_a_big_brush)
{
    Cel *plain[2];
    for (int perfect = 0; perfect < 2; perfect++) {
        History *h = history_create();
        plain[perfect] = cel_create(W, H);
        PencilStroke *s = pencil_stroke_begin_ex(
            h, plain[perfect], RED, 0, (PencilOptions){.size = 2, .pixel_perfect = perfect}, 0, 0);
        pencil_stroke_extend(s, 1, 0);
        pencil_stroke_extend(s, 1, 1);
        pencil_stroke_extend(s, 2, 1);
        pencil_stroke_end(s);
        history_destroy(h);
    }
    ASSERT_EQ_INT(0, memcmp(cel_pixels(plain[0]), cel_pixels(plain[1]),
                            (size_t)W * (size_t)H * sizeof(Rgba)));
    cel_destroy(plain[0]);
    cel_destroy(plain[1]);
}

TEST(oversized_brushes_are_capped)
{
    Cel *c = dab((PencilOptions){.size = 100000}, 3, 2);
    ASSERT_EQ_INT(255, cel_get(c, 0, 0).a);
    ASSERT_EQ_INT(255, cel_get(c, W - 1, H - 1).a);
    cel_destroy(c);
    c = dab((PencilOptions){.size = -4}, 3, 2);
    const char *golden[H] = {"........", "........", "...#....", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
}

TEST(brush_helpers_describe_the_footprint)
{
    ASSERT_EQ_INT(1, pencil_brush_size(&(PencilOptions){0}));
    ASSERT_EQ_INT(5, pencil_brush_size(&(PencilOptions){.size = 5}));
    ASSERT_EQ_INT(PENCIL_BRUSH_MAX, pencil_brush_size(&(PencilOptions){.size = 9999}));
    PencilOptions round = {.size = 3, .round = 1};
    ASSERT_TRUE(!pencil_brush_contains(&round, 0, 0));
    ASSERT_TRUE(pencil_brush_contains(&round, 1, 0));
    ASSERT_TRUE(pencil_brush_contains(&round, 1, 1));
    ASSERT_TRUE(!pencil_brush_contains(&round, 3, 1)); /* outside the brush box */
    ASSERT_TRUE(!pencil_brush_contains(&round, -1, 1));
    ASSERT_TRUE(pencil_brush_contains(&(PencilOptions){.size = 3}, 0, 0));
    ASSERT_EQ_INT(0, pencil_brush_origin(&(PencilOptions){.size = 2}));
    ASSERT_EQ_INT(1, pencil_brush_origin(&(PencilOptions){.size = 3}));
    ASSERT_EQ_INT(1, pencil_brush_origin(&(PencilOptions){.size = 4}));
}

int main(void)
{
    RUN(brush_helpers_describe_the_footprint);
    RUN(size_zero_and_one_are_a_single_pixel);
    RUN(even_square_brush_grows_right_and_down);
    RUN(odd_square_brush_is_centered);
    RUN(round_brush_rounds_the_corners);
    RUN(brush_is_clipped_at_the_cel_edge);
    RUN(dragging_sweeps_the_brush_along_the_line);
    RUN(mirrored_brush_reflects_every_pixel_of_the_stamp);
    RUN(eraser_uses_the_brush_too);
    RUN(pixel_perfect_is_ignored_for_a_big_brush);
    RUN(oversized_brushes_are_capped);
    return TEST_SUMMARY();
}
