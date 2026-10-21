#include "minitest.h"
#include "tinysprite/history.h"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

static int alpha(const Cel *c, int x, int y)
{
    return cel_get(c, x, y).a;
}

TEST(starts_empty)
{
    History *h = history_create();
    ASSERT_TRUE(h != NULL);
    ASSERT_TRUE(!history_can_undo(h));
    ASSERT_TRUE(!history_can_redo(h));
    ASSERT_EQ_INT(-1, history_undo(h));
    ASSERT_EQ_INT(-1, history_redo(h));
    history_destroy(h);
    history_destroy(NULL);
}

TEST(set_pixel_needs_active_stroke)
{
    History *h = history_create();
    Cel *c = cel_create(4, 4);
    ASSERT_EQ_INT(-1, history_set_pixel(h, 0, 0, RED));
    ASSERT_EQ_INT(0, alpha(c, 0, 0));
    ASSERT_EQ_INT(-1, history_end_stroke(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(stroke_writes_pixels_and_undoes_as_one_step)
{
    History *h = history_create();
    Cel *c = cel_create(4, 4);
    ASSERT_EQ_INT(0, history_begin_stroke(h, c));
    ASSERT_EQ_INT(0, history_set_pixel(h, 0, 0, RED));
    ASSERT_EQ_INT(0, history_set_pixel(h, 1, 0, RED));
    ASSERT_EQ_INT(0, history_set_pixel(h, 2, 0, RED));
    ASSERT_EQ_INT(255, alpha(c, 1, 0));
    ASSERT_EQ_INT(0, history_end_stroke(h));
    ASSERT_TRUE(history_can_undo(h));

    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(0, alpha(c, 0, 0));
    ASSERT_EQ_INT(0, alpha(c, 1, 0));
    ASSERT_EQ_INT(0, alpha(c, 2, 0));
    ASSERT_TRUE(!history_can_undo(h));
    ASSERT_TRUE(history_can_redo(h));

    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(255, alpha(c, 0, 0));
    ASSERT_EQ_INT(255, alpha(c, 2, 0));
    ASSERT_TRUE(!history_can_redo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(pixel_written_twice_restores_original)
{
    History *h = history_create();
    Cel *c = cel_create(2, 2);
    cel_set(c, 0, 0, BLUE);
    history_begin_stroke(h, c);
    history_set_pixel(h, 0, 0, RED);
    history_set_pixel(h, 0, 0, RED);
    history_set_pixel(h, 0, 0, BLUE);
    history_set_pixel(h, 0, 0, RED);
    history_end_stroke(h);
    ASSERT_EQ_INT(255, cel_get(c, 0, 0).r);

    history_undo(h);
    ASSERT_EQ_INT(0, cel_get(c, 0, 0).r);
    ASSERT_EQ_INT(255, cel_get(c, 0, 0).b);
    history_redo(h);
    ASSERT_EQ_INT(255, cel_get(c, 0, 0).r);
    cel_destroy(c);
    history_destroy(h);
}

TEST(multiple_strokes_undo_in_reverse_order)
{
    History *h = history_create();
    Cel *c = cel_create(3, 1);
    for (int x = 0; x < 3; x++) {
        history_begin_stroke(h, c);
        history_set_pixel(h, x, 0, RED);
        history_end_stroke(h);
    }
    history_undo(h);
    ASSERT_EQ_INT(255, alpha(c, 1, 0));
    ASSERT_EQ_INT(0, alpha(c, 2, 0));
    history_undo(h);
    ASSERT_EQ_INT(255, alpha(c, 0, 0));
    ASSERT_EQ_INT(0, alpha(c, 1, 0));
    history_undo(h);
    ASSERT_EQ_INT(0, alpha(c, 0, 0));
    ASSERT_EQ_INT(-1, history_undo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(new_stroke_discards_redo)
{
    History *h = history_create();
    Cel *c = cel_create(3, 1);
    history_begin_stroke(h, c);
    history_set_pixel(h, 0, 0, RED);
    history_end_stroke(h);
    history_undo(h);
    ASSERT_TRUE(history_can_redo(h));

    history_begin_stroke(h, c);
    history_set_pixel(h, 1, 0, RED);
    history_end_stroke(h);
    ASSERT_TRUE(!history_can_redo(h));
    ASSERT_EQ_INT(-1, history_redo(h));
    ASSERT_EQ_INT(0, alpha(c, 0, 0));
    ASSERT_EQ_INT(255, alpha(c, 1, 0));
    cel_destroy(c);
    history_destroy(h);
}

TEST(empty_and_noop_strokes_are_dropped)
{
    History *h = history_create();
    Cel *c = cel_create(2, 2);
    history_begin_stroke(h, c);
    ASSERT_EQ_INT(0, history_end_stroke(h));
    ASSERT_TRUE(!history_can_undo(h));

    cel_set(c, 0, 0, RED);
    history_begin_stroke(h, c);
    history_set_pixel(h, 0, 0, RED);
    history_end_stroke(h);
    ASSERT_TRUE(!history_can_undo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(out_of_bounds_pixels_are_rejected)
{
    History *h = history_create();
    Cel *c = cel_create(2, 2);
    history_begin_stroke(h, c);
    ASSERT_EQ_INT(-1, history_set_pixel(h, 2, 0, RED));
    ASSERT_EQ_INT(-1, history_set_pixel(h, 0, -1, RED));
    history_end_stroke(h);
    ASSERT_TRUE(!history_can_undo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(only_one_stroke_at_a_time)
{
    History *h = history_create();
    Cel *c = cel_create(2, 2);
    ASSERT_EQ_INT(0, history_begin_stroke(h, c));
    ASSERT_EQ_INT(-1, history_begin_stroke(h, c));
    history_set_pixel(h, 0, 0, RED);
    ASSERT_EQ_INT(-1, history_undo(h));
    ASSERT_EQ_INT(-1, history_redo(h));
    ASSERT_EQ_INT(0, history_end_stroke(h));
    ASSERT_EQ_INT(-1, history_begin_stroke(NULL, c));
    ASSERT_EQ_INT(-1, history_begin_stroke(h, NULL));
    cel_destroy(c);
    history_destroy(h);
}

TEST(strokes_on_different_cels)
{
    History *h = history_create();
    Cel *a = cel_create(2, 2);
    Cel *b = cel_create(2, 2);
    history_begin_stroke(h, a);
    history_set_pixel(h, 0, 0, RED);
    history_end_stroke(h);
    history_begin_stroke(h, b);
    history_set_pixel(h, 1, 1, BLUE);
    history_end_stroke(h);

    history_undo(h);
    ASSERT_EQ_INT(255, alpha(a, 0, 0));
    ASSERT_EQ_INT(0, alpha(b, 1, 1));
    history_undo(h);
    ASSERT_EQ_INT(0, alpha(a, 0, 0));
    cel_destroy(a);
    cel_destroy(b);
    history_destroy(h);
}

TEST(destroy_with_active_stroke_and_redo_is_clean)
{
    History *h = history_create();
    Cel *c = cel_create(2, 2);
    history_begin_stroke(h, c);
    history_set_pixel(h, 0, 0, RED);
    history_end_stroke(h);
    history_undo(h);
    history_begin_stroke(h, c);
    history_set_pixel(h, 1, 1, RED);
    history_destroy(h);
    cel_destroy(c);
}

TEST(many_strokes_grow_storage)
{
    History *h = history_create();
    Cel *c = cel_create(64, 64);
    for (int i = 0; i < 1000; i++) {
        history_begin_stroke(h, c);
        history_set_pixel(h, i % 64, (i / 64) % 64, (i & 1) ? RED : BLUE);
        history_end_stroke(h);
    }
    for (int i = 0; i < 1000; i++) {
        ASSERT_EQ_INT(0, history_undo(h));
    }
    ASSERT_EQ_INT(-1, history_undo(h));
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            ASSERT_EQ_INT(0, alpha(c, x, y));
        }
    }
    cel_destroy(c);
    history_destroy(h);
}

TEST(cancel_stroke_restores_pixels_and_keeps_history)
{
    History *h = history_create();
    Cel *c = cel_create(4, 4);
    Rgba red = {255, 0, 0, 255}, blue = {0, 0, 255, 255};
    ASSERT_EQ_INT(0, history_begin_stroke(h, c));
    ASSERT_EQ_INT(0, history_set_pixel(h, 1, 1, red));
    ASSERT_EQ_INT(0, history_end_stroke(h));
    ASSERT_EQ_INT(0, history_begin_stroke(h, c));
    ASSERT_EQ_INT(0, history_set_pixel(h, 1, 1, blue));
    ASSERT_EQ_INT(0, history_set_pixel(h, 2, 2, blue));
    ASSERT_EQ_INT(0, history_cancel_stroke(h));
    ASSERT_EQ_INT(red.r, cel_get(c, 1, 1).r);
    ASSERT_EQ_INT(0, cel_get(c, 1, 1).b);
    ASSERT_EQ_INT(0, cel_get(c, 2, 2).a);
    ASSERT_EQ_INT(-1, history_end_stroke(h));
    ASSERT_EQ_INT(-1, history_cancel_stroke(h));
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_TRUE(!history_can_undo(h));
    cel_destroy(c);
    history_destroy(h);
}

TEST(forget_cel_drops_only_that_cels_steps)
{
    History *h = history_create();
    Cel *a = cel_create(4, 4);
    Cel *b = cel_create(4, 4);
    Rgba red = {255, 0, 0, 255};
    history_begin_stroke(h, a);
    history_set_pixel(h, 0, 0, red);
    history_end_stroke(h);
    history_begin_stroke(h, b);
    history_set_pixel(h, 1, 1, red);
    history_end_stroke(h);
    history_begin_stroke(h, a);
    history_set_pixel(h, 2, 2, red);
    history_end_stroke(h);

    history_forget_cel(h, a);
    cel_destroy(a);
    ASSERT_EQ_INT(0, history_undo(h)); /* only b's stroke is left */
    ASSERT_EQ_INT(0, cel_get(b, 1, 1).a);
    ASSERT_TRUE(!history_can_undo(h));
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(255, cel_get(b, 1, 1).a);
    ASSERT_TRUE(!history_can_redo(h));
    cel_destroy(b);
    history_destroy(h);
}

TEST(forget_cel_keeps_undo_redo_boundary)
{
    History *h = history_create();
    Cel *a = cel_create(4, 4);
    Cel *b = cel_create(4, 4);
    Rgba red = {255, 0, 0, 255};
    history_begin_stroke(h, a);
    history_set_pixel(h, 0, 0, red);
    history_end_stroke(h);
    history_begin_stroke(h, b);
    history_set_pixel(h, 1, 1, red);
    history_end_stroke(h);
    history_begin_stroke(h, a);
    history_set_pixel(h, 2, 2, red);
    history_end_stroke(h);
    history_undo(h); /* a(2,2) is redoable; a(0,0) and b(1,1) are applied */

    history_forget_cel(h, a);
    cel_destroy(a);
    ASSERT_TRUE(!history_can_redo(h));
    ASSERT_TRUE(history_can_undo(h));
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(0, cel_get(b, 1, 1).a);
    cel_destroy(b);
    history_destroy(h);
}

TEST(forget_cel_cancels_active_stroke_on_it)
{
    History *h = history_create();
    Cel *a = cel_create(4, 4);
    Rgba red = {255, 0, 0, 255};
    history_begin_stroke(h, a);
    history_set_pixel(h, 0, 0, red);
    history_forget_cel(h, a);
    ASSERT_EQ_INT(0, cel_get(a, 0, 0).a);
    ASSERT_EQ_INT(-1, history_end_stroke(h));
    ASSERT_EQ_INT(0, history_begin_stroke(h, a));
    history_end_stroke(h);
    cel_destroy(a);
    history_destroy(h);
}

typedef struct Counter {
    int undos, redos, releases;
} Counter;

static void counter_undo(void *ctx)
{
    ((Counter *)ctx)->undos++;
}

static void counter_redo(void *ctx)
{
    ((Counter *)ctx)->redos++;
}

static void counter_release(void *ctx)
{
    ((Counter *)ctx)->releases++;
}

static HistoryAction counter_action(Counter *counter)
{
    return (HistoryAction){counter_undo, counter_redo, counter_release, counter};
}

TEST(action_undo_redo_calls_callbacks)
{
    History *h = history_create();
    Counter n = {0};
    ASSERT_EQ_INT(0, history_push_action(h, counter_action(&n)));
    ASSERT_TRUE(history_can_undo(h));
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(1, n.undos);
    ASSERT_TRUE(history_can_redo(h));
    ASSERT_EQ_INT(0, history_redo(h));
    ASSERT_EQ_INT(1, n.redos);
    ASSERT_EQ_INT(0, n.releases);
    history_destroy(h);
    ASSERT_EQ_INT(1, n.releases);
}

TEST(action_interleaves_with_strokes)
{
    History *h = history_create();
    Cel *c = cel_create(4, 4);
    Counter n = {0};
    history_begin_stroke(h, c);
    history_set_pixel(h, 0, 0, RED);
    history_end_stroke(h);
    history_push_action(h, counter_action(&n));
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(255, alpha(c, 0, 0));
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(0, alpha(c, 0, 0));
    ASSERT_EQ_INT(1, n.undos);
    history_destroy(h);
    cel_destroy(c);
}

TEST(new_edit_releases_redone_actions)
{
    History *h = history_create();
    Cel *c = cel_create(4, 4);
    Counter n = {0};
    history_push_action(h, counter_action(&n));
    history_undo(h);
    history_begin_stroke(h, c);
    history_set_pixel(h, 1, 1, BLUE);
    history_end_stroke(h);
    ASSERT_EQ_INT(1, n.releases);
    history_destroy(h);
    ASSERT_EQ_INT(1, n.releases);
    cel_destroy(c);
}

TEST(action_rejected_during_stroke_is_released)
{
    History *h = history_create();
    Cel *c = cel_create(4, 4);
    Counter n = {0};
    history_begin_stroke(h, c);
    ASSERT_EQ_INT(-1, history_push_action(h, counter_action(&n)));
    ASSERT_EQ_INT(1, n.releases);
    ASSERT_EQ_INT(-1, history_push_action(NULL, counter_action(&n)));
    ASSERT_EQ_INT(2, n.releases);
    history_end_stroke(h);
    history_destroy(h);
    cel_destroy(c);
}

TEST(forget_cel_keeps_actions)
{
    History *h = history_create();
    Cel *c = cel_create(4, 4);
    Counter n = {0};
    history_begin_stroke(h, c);
    history_set_pixel(h, 0, 0, RED);
    history_end_stroke(h);
    history_push_action(h, counter_action(&n));
    history_forget_cel(h, c);
    ASSERT_EQ_INT(0, n.releases);
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT(1, n.undos);
    ASSERT_TRUE(!history_can_undo(h));
    history_destroy(h);
    cel_destroy(c);
}

int main(void)
{
    RUN(starts_empty);
    RUN(set_pixel_needs_active_stroke);
    RUN(stroke_writes_pixels_and_undoes_as_one_step);
    RUN(pixel_written_twice_restores_original);
    RUN(multiple_strokes_undo_in_reverse_order);
    RUN(new_stroke_discards_redo);
    RUN(empty_and_noop_strokes_are_dropped);
    RUN(out_of_bounds_pixels_are_rejected);
    RUN(only_one_stroke_at_a_time);
    RUN(strokes_on_different_cels);
    RUN(destroy_with_active_stroke_and_redo_is_clean);
    RUN(many_strokes_grow_storage);
    RUN(cancel_stroke_restores_pixels_and_keeps_history);
    RUN(forget_cel_drops_only_that_cels_steps);
    RUN(forget_cel_keeps_undo_redo_boundary);
    RUN(forget_cel_cancels_active_stroke_on_it);
    RUN(action_undo_redo_calls_callbacks);
    RUN(action_interleaves_with_strokes);
    RUN(new_edit_releases_redone_actions);
    RUN(action_rejected_during_stroke_is_released);
    RUN(forget_cel_keeps_actions);
    return TEST_SUMMARY();
}
