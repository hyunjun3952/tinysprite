#include <cstring>

#include "minitest.hpp"
#include "tinysprite/selection.hpp"

#define W 8
#define H 6

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

static void expect_rows(const Cel &c, const char *const golden[H])
{
    for (int y = 0; y < H; y++) {
        char row[W + 1];
        for (int x = 0; x < W; x++) {
            Rgba p = c.get(x, y);
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
static Cel block()
{
    Cel c(W, H);
    for (int y = 1; y <= 2; y++) {
        for (int x = 1; x <= 2; x++) {
            c.set(x, y, RED);
        }
    }
    return c;
}

TEST(moves_the_selection_and_clears_the_source)
{
    History h;
    Cel c = block();
    ASSERT_TRUE(move_stroke(h, c, Rect{1, 1, 2, 2}, 3, 2));
    const char *golden[H] = {"........", "........", "........",
                             "....rr..", "....rr..", "........"};
    expect_rows(c, golden);
}

TEST(overlapping_move_keeps_the_pixels_that_stay_covered)
{
    History h;
    Cel c = block();
    ASSERT_TRUE(move_stroke(h, c, Rect{1, 1, 2, 2}, 1, 0));
    const char *golden[H] = {"........", "..rr....", "..rr....",
                             "........", "........", "........"};
    expect_rows(c, golden);
}

TEST(only_the_selected_area_moves)
{
    History h;
    Cel c = block();
    c.set(5, 5, BLUE);
    ASSERT_TRUE(move_stroke(h, c, Rect{1, 1, 1, 2}, 0, 3)); /* left column of the block */
    const char *golden[H] = {"........", "..r.....", "..r.....",
                             "........", ".r......", ".r...b.."};
    expect_rows(c, golden);
}

TEST(transparent_pixels_do_not_erase_the_destination)
{
    History h;
    Cel c(W, H);
    c.set(0, 0, RED);  /* selection (0,0)-(1,0): red then empty */
    c.set(5, 0, BLUE); /* sits where the empty half lands */
    ASSERT_TRUE(move_stroke(h, c, Rect{0, 0, 2, 1}, 4, 0));
    const char *golden[H] = {"....rb..", "........", "........",
                             "........", "........", "........"};
    expect_rows(c, golden);
}

TEST(pixels_moved_off_the_cel_are_lost)
{
    History h;
    Cel c = block();
    ASSERT_TRUE(move_stroke(h, c, Rect{1, 1, 2, 2}, 6, 4));
    const char *golden[H] = {"........", "........", "........",
                             "........", "........", ".......r"};
    expect_rows(c, golden);
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(255, c.get(1, 1).a);
    ASSERT_EQ_INT(255, c.get(2, 2).a);
}

TEST(selection_is_clipped_to_the_cel)
{
    History h;
    Cel c(W, H);
    c.set(0, 0, RED);
    ASSERT_TRUE(move_stroke(h, c, Rect{-3, -3, 5, 5}, 2, 1));
    ASSERT_EQ_INT(0, c.get(0, 0).a);
    ASSERT_EQ_INT(255, c.get(2, 1).a);
}

TEST(partial_colors_are_copied_exactly)
{
    History h;
    Cel c(W, H);
    c.set(0, 0, Rgba{10, 20, 30, 40});
    move_stroke(h, c, Rect{0, 0, 1, 1}, 2, 2);
    Rgba p = c.get(2, 2);
    ASSERT_EQ_INT(10, p.r);
    ASSERT_EQ_INT(20, p.g);
    ASSERT_EQ_INT(30, p.b);
    ASSERT_EQ_INT(40, p.a);
}

TEST(update_previews_replace_each_other)
{
    History h;
    Cel c = block();
    auto m = MoveStroke::begin(h, c, Rect{1, 1, 2, 2});
    m->update(4, 0);
    m->update(0, 3);
    m->update(2, 2);
    ASSERT_TRUE(m->end());
    const char *golden[H] = {"........", "........", "........",
                             "...rr...", "...rr...", "........"};
    expect_rows(c, golden);
}

TEST(whole_drag_is_one_undo_step)
{
    History h;
    Cel c = block();
    auto m = MoveStroke::begin(h, c, Rect{1, 1, 2, 2});
    m->update(1, 1);
    m->update(3, 3);
    m->end();
    ASSERT_TRUE(h.undo());
    const char *original[H] = {"........", ".rr.....", ".rr.....",
                               "........", "........", "........"};
    expect_rows(c, original);
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(255, c.get(4, 4).a);
}

TEST(zero_offset_records_nothing)
{
    History h;
    Cel c = block();
    auto m = MoveStroke::begin(h, c, Rect{1, 1, 2, 2});
    m->update(2, 2);
    m->update(0, 0);
    ASSERT_TRUE(m->end());
    ASSERT_TRUE(!h.canUndo());
    ASSERT_EQ_INT(255, c.get(1, 1).a);
    ASSERT_EQ_INT(0, c.get(3, 3).a);
}

TEST(cancel_restores_the_cel_and_records_nothing)
{
    History h;
    Cel c = block();
    auto m = MoveStroke::begin(h, c, Rect{1, 1, 2, 2});
    m->update(3, 3);
    m->cancel();
    const char *original[H] = {"........", ".rr.....", ".rr.....",
                               "........", "........", "........"};
    expect_rows(c, original);
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.beginStroke(c));
    h.cancelStroke();
}

TEST(invalid_arguments_fail)
{
    History h;
    Cel c = block();
    ASSERT_TRUE(MoveStroke::begin(h, c, Rect{0, 0, 0, 2}) == nullptr);
    ASSERT_TRUE(MoveStroke::begin(h, c, Rect{0, 0, 2, -1}) == nullptr);
    ASSERT_TRUE(MoveStroke::begin(h, c, Rect{20, 20, 2, 2}) ==
                nullptr); /* nothing inside the cel */
    auto m = MoveStroke::begin(h, c, Rect{1, 1, 2, 2});
    ASSERT_TRUE(MoveStroke::begin(h, c, Rect{1, 1, 2, 2}) == nullptr); /* one at a time */
    ASSERT_TRUE(!move_stroke(h, c, Rect{1, 1, 2, 2}, 1, 1));
    m->cancel();
}

int main()
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
