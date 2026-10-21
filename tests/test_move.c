#include <string.h>

#include "minitest.h"
#include "tinysprite/move.h"

#define W 8
#define H 6

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

static void expect_rows(const Cel *c, const char *const golden[H])
{
    for (int y = 0; y < H; y++) {
        char row[W + 1];
        for (int x = 0; x < W; x++) {
            Rgba p = cel_get(c, x, y);
            char glyph = 'r';
            if (p.a == 0) {
                glyph = '.';
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

/* A 2x2 red block at (1,1)-(2,2). */
static Cel *block(void)
{
    Cel *c = cel_create(W, H);
    for (int y = 1; y <= 2; y++) {
        for (int x = 1; x <= 2; x++) {
            cel_set(c, x, y, RED);
        }
    }
    return c;
}

TEST(moves_the_selection_and_clears_the_source)
{
    History *h = history_create();
    Cel *c = block();
    ASSERT_EQ_INT(0, move_stroke(h, c, (Rect){1, 1, 2, 2}, 3, 2));
    const char *golden[H] = {"........", "........", "........",
                             "....rr..", "....rr..", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(overlapping_move_keeps_the_pixels_that_stay_covered)
{
    History *h = history_create();
    Cel *c = block();
    ASSERT_EQ_INT(0, move_stroke(h, c, (Rect){1, 1, 2, 2}, 1, 0));
    const char *golden[H] = {"........", "..rr....", "..rr....",
                             "........", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(only_the_selected_area_moves)
{
    History *h = history_create();
    Cel *c = block();
    cel_set(c, 5, 5, BLUE);
    ASSERT_EQ_INT(0, move_stroke(h, c, (Rect){1, 1, 1, 2}, 0, 3)); /* left column of the block */
    const char *golden[H] = {"........", "..r.....", "..r.....",
                             "........", ".r......", ".r...b.."};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(transparent_pixels_do_not_erase_the_destination)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    cel_set(c, 0, 0, RED);  /* selection (0,0)-(1,0): red then empty */
    cel_set(c, 5, 0, BLUE); /* sits where the empty half lands */
    ASSERT_EQ_INT(0, move_stroke(h, c, (Rect){0, 0, 2, 1}, 4, 0));
    const char *golden[H] = {"....rb..", "........", "........",
                             "........", "........", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(pixels_moved_off_the_cel_are_lost)
{
    History *h = history_create();
    Cel *c = block();
    ASSERT_EQ_INT(0, move_stroke(h, c, (Rect){1, 1, 2, 2}, 6, 4));
    const char *golden[H] = {"........", "........", "........",
                             "........", "........", ".......r"};
    expect_rows(c, golden);
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(255, cel_get(c, 1, 1).a);
    ASSERT_EQ_INT(255, cel_get(c, 2, 2).a);
    cel_destroy(c);
    history_destroy(h);
}

TEST(selection_is_clipped_to_the_cel)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    cel_set(c, 0, 0, RED);
    ASSERT_EQ_INT(0, move_stroke(h, c, (Rect){-3, -3, 5, 5}, 2, 1));
    ASSERT_EQ_INT(0, cel_get(c, 0, 0).a);
    ASSERT_EQ_INT(255, cel_get(c, 2, 1).a);
    cel_destroy(c);
    history_destroy(h);
}

TEST(partial_colors_are_copied_exactly)
{
    History *h = history_create();
    Cel *c = cel_create(W, H);
    cel_set(c, 0, 0, (Rgba){10, 20, 30, 40});
    move_stroke(h, c, (Rect){0, 0, 1, 1}, 2, 2);
    Rgba p = cel_get(c, 2, 2);
    ASSERT_EQ_INT(10, p.r);
    ASSERT_EQ_INT(20, p.g);
    ASSERT_EQ_INT(30, p.b);
    ASSERT_EQ_INT(40, p.a);
    cel_destroy(c);
    history_destroy(h);
}

TEST(update_previews_replace_each_other)
{
    History *h = history_create();
    Cel *c = block();
    MoveStroke *m = move_stroke_begin(h, c, (Rect){1, 1, 2, 2});
    ASSERT_TRUE(m != NULL);
    ASSERT_EQ_INT(0, move_stroke_update(m, 4, 0));
    ASSERT_EQ_INT(0, move_stroke_update(m, 0, 3));
    ASSERT_EQ_INT(0, move_stroke_update(m, 2, 2));
    ASSERT_EQ_INT(0, move_stroke_end(m));
    const char *golden[H] = {"........", "........", "........",
                             "...rr...", "...rr...", "........"};
    expect_rows(c, golden);
    cel_destroy(c);
    history_destroy(h);
}

TEST(whole_drag_is_one_undo_step)
{
    History *h = history_create();
    Cel *c = block();
    MoveStroke *m = move_stroke_begin(h, c, (Rect){1, 1, 2, 2});
    move_stroke_update(m, 1, 1);
    move_stroke_update(m, 3, 3);
    move_stroke_end(m);
    ASSERT_EQ_INT(0, history_undo(h));
    const char *original[H] = {"........", ".rr.....", ".rr.....",
                               "........", "........", "........"};
    expect_rows(c, original);
    ASSERT_TRUE(!history_can_undo(h));
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(255, cel_get(c, 4, 4).a);
    cel_destroy(c);
    history_destroy(h);
}

TEST(zero_offset_records_nothing)
{
    History *h = history_create();
    Cel *c = block();
    MoveStroke *m = move_stroke_begin(h, c, (Rect){1, 1, 2, 2});
    ASSERT_EQ_INT(0, move_stroke_update(m, 2, 2));
    ASSERT_EQ_INT(0, move_stroke_update(m, 0, 0));
    ASSERT_EQ_INT(0, move_stroke_end(m));
    ASSERT_TRUE(!history_can_undo(h));
    ASSERT_EQ_INT(255, cel_get(c, 1, 1).a);
    ASSERT_EQ_INT(0, cel_get(c, 3, 3).a);
    cel_destroy(c);
    history_destroy(h);
}

TEST(cancel_restores_the_cel_and_records_nothing)
{
    History *h = history_create();
    Cel *c = block();
    MoveStroke *m = move_stroke_begin(h, c, (Rect){1, 1, 2, 2});
    move_stroke_update(m, 3, 3);
    move_stroke_cancel(m);
    const char *original[H] = {"........", ".rr.....", ".rr.....",
                               "........", "........", "........"};
    expect_rows(c, original);
    ASSERT_TRUE(!history_can_undo(h));
    ASSERT_EQ_INT(0, history_begin_stroke(h, c));
    history_cancel_stroke(h);
    cel_destroy(c);
    history_destroy(h);
}

TEST(invalid_arguments_fail)
{
    History *h = history_create();
    Cel *c = block();
    ASSERT_TRUE(move_stroke_begin(NULL, c, (Rect){0, 0, 2, 2}) == NULL);
    ASSERT_TRUE(move_stroke_begin(h, NULL, (Rect){0, 0, 2, 2}) == NULL);
    ASSERT_TRUE(move_stroke_begin(h, c, (Rect){0, 0, 0, 2}) == NULL);
    ASSERT_TRUE(move_stroke_begin(h, c, (Rect){0, 0, 2, -1}) == NULL);
    ASSERT_TRUE(move_stroke_begin(h, c, (Rect){20, 20, 2, 2}) == NULL); /* nothing inside the cel */
    MoveStroke *m = move_stroke_begin(h, c, (Rect){1, 1, 2, 2});
    ASSERT_TRUE(move_stroke_begin(h, c, (Rect){1, 1, 2, 2}) == NULL); /* one at a time */
    ASSERT_EQ_INT(-1, move_stroke(h, c, (Rect){1, 1, 2, 2}, 1, 1));
    ASSERT_EQ_INT(-1, move_stroke_update(NULL, 1, 1));
    ASSERT_EQ_INT(-1, move_stroke_end(NULL));
    move_stroke_cancel(NULL);
    move_stroke_cancel(m);
    cel_destroy(c);
    history_destroy(h);
}

int main(void)
{
    RUN(moves_the_selection_and_clears_the_source);
    RUN(overlapping_move_keeps_the_pixels_that_stay_covered);
    RUN(only_the_selected_area_moves);
    RUN(transparent_pixels_do_not_erase_the_destination);
    RUN(pixels_moved_off_the_cel_are_lost);
    RUN(selection_is_clipped_to_the_cel);
    RUN(partial_colors_are_copied_exactly);
    RUN(update_previews_replace_each_other);
    RUN(whole_drag_is_one_undo_step);
    RUN(zero_offset_records_nothing);
    RUN(cancel_restores_the_cel_and_records_nothing);
    RUN(invalid_arguments_fail);
    return TEST_SUMMARY();
}
