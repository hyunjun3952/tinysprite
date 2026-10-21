#include <string.h>

#include "minitest.h"
#include "tinysprite/bucket.h"
#include "tinysprite/pencil.h"

#define W 6
#define H 4

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

/* Renders the cel as rows: '.' transparent, 'r' red, 'b' blue, '#' anything else. */
static void expect_rows(const Cel *c, const char *const golden[H])
{
    for (int y = 0; y < H; y++) {
        char row[W + 1];
        for (int x = 0; x < W; x++) {
            Rgba p = cel_get(c, x, y);
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
static void build_wall(History *h, Cel *c)
{
    ToolPoint wall[] = {{2, 0}, {2, 3}};
    pencil_stroke(h, c, wall, 2, RED);
}

TEST(fills_the_connected_region_only)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    build_wall(h, c);
    ASSERT_EQ_INT(0, bucket_fill(h, c, 0, 0, BLUE, SYMMETRY_NONE));
    const char *golden[H] = {"bbr...", "bbr...", "bbr...", "bbr..."};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(fill_is_one_undo_step)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    build_wall(h, c);
    bucket_fill(h, c, 4, 1, BLUE, SYMMETRY_NONE);
    ASSERT_EQ_INT(0, history_undo(h));
    const char *after_undo[H] = {"..r...", "..r...", "..r...", "..r..."};
    expect_rows(c, after_undo);
    ASSERT_EQ_INT(0, history_redo(h));
    const char *after_redo[H] = {"..rbbb", "..rbbb", "..rbbb", "..rbbb"};
    expect_rows(c, after_redo);
    cel_destroy(c);
    history_destroy(h);
}

TEST(filling_with_the_same_color_records_nothing)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ASSERT_EQ_INT(0, bucket_fill(h, c, 0, 0, (Rgba){0, 0, 0, 0}, SYMMETRY_NONE));
    ASSERT_TRUE(!history_can_undo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(fill_replaces_with_transparent_too)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    build_wall(h, c);
    bucket_fill(h, c, 2, 2, (Rgba){0, 0, 0, 0}, SYMMETRY_NONE);
    const char *golden[H] = {"......", "......", "......", "......"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(invalid_arguments_and_active_stroke_fail)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    ASSERT_EQ_INT(-1, bucket_fill(h, c, -1, 0, BLUE, SYMMETRY_NONE));
    ASSERT_EQ_INT(-1, bucket_fill(h, c, 0, H, BLUE, SYMMETRY_NONE));
    ASSERT_EQ_INT(-1, bucket_fill(NULL, c, 0, 0, BLUE, SYMMETRY_NONE));
    ASSERT_EQ_INT(-1, bucket_fill(h, NULL, 0, 0, BLUE, SYMMETRY_NONE));
    history_begin_stroke(h, c);
    ASSERT_EQ_INT(-1, bucket_fill(h, c, 0, 0, BLUE, SYMMETRY_NONE));
    history_cancel_stroke(h);
    ASSERT_TRUE(!history_can_undo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(mirror_x_fills_the_reflected_region_too)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    /* A blue wall splits the cel into a left and a right region. */
    for (int y = 0; y < H; y++) {
        cel_set(c, 2, y, BLUE);
        cel_set(c, 3, y, BLUE);
    }
    Symmetry sym = {.mirror_x = 1};
    ASSERT_EQ_INT(0, bucket_fill(h, c, 0, 0, RED, sym));
    const char *golden[H] = {"rrbbrr", "rrbbrr", "rrbbrr", "rrbbrr"};
    expect_rows(c, golden);
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(0, cel_get(c, 0, 0).a);
    ASSERT_EQ_INT(0, cel_get(c, 5, 0).a);
    cel_destroy(c);
    history_destroy(h);
}

TEST(mirror_seed_on_the_axis_fills_once)
{
    History *h = history_create();
    Cel *c = cel_create(5, 4);
    Symmetry sym = {.mirror_x = 1, .mirror_y = 1};
    ASSERT_EQ_INT(0, bucket_fill(h, c, 2, 1, RED, sym));
    ASSERT_EQ_INT(255, cel_get(c, 0, 0).a);
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(0, cel_get(c, 0, 0).a);
    ASSERT_TRUE(!history_can_undo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(mirror_x_with_four_corners_fills_four_regions)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    for (int x = 0; x < W; x++) {
        cel_set(c, x, 1, BLUE);
        cel_set(c, x, 2, BLUE);
    }
    for (int y = 0; y < H; y++) {
        cel_set(c, 2, y, BLUE);
        cel_set(c, 3, y, BLUE);
    }
    Symmetry sym = {.mirror_x = 1, .mirror_y = 1};
    ASSERT_EQ_INT(0, bucket_fill(h, c, 0, 0, RED, sym));
    ASSERT_EQ_INT(255, cel_get(c, 0, 0).r);
    ASSERT_EQ_INT(255, cel_get(c, 5, 0).r);
    ASSERT_EQ_INT(255, cel_get(c, 0, 3).r);
    ASSERT_EQ_INT(255, cel_get(c, 5, 3).r);
    cel_destroy(c);
    history_destroy(h);
}

int main(void)
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
