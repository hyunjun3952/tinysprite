#include <memory>

#include "minitest.h"
#include "tinysprite/history.h"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

static int alpha(const Cel &c, int x, int y)
{
    return c.get(x, y).a;
}

TEST(starts_empty)
{
    History h;
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(!h.canRedo());
    ASSERT_TRUE(!h.undo());
    ASSERT_TRUE(!h.redo());
}

TEST(set_pixel_needs_active_stroke)
{
    History h;
    Cel c(4, 4);
    ASSERT_TRUE(!h.setPixel(0, 0, RED));
    ASSERT_EQ_INT(0, alpha(c, 0, 0));
    ASSERT_TRUE(!h.endStroke());
}

TEST(stroke_writes_pixels_and_undoes_as_one_step)
{
    History h;
    Cel c(4, 4);
    ASSERT_TRUE(h.beginStroke(c));
    ASSERT_TRUE(h.setPixel(0, 0, RED));
    ASSERT_TRUE(h.setPixel(1, 0, RED));
    ASSERT_TRUE(h.setPixel(2, 0, RED));
    ASSERT_EQ_INT(255, alpha(c, 1, 0));
    ASSERT_TRUE(h.endStroke());
    ASSERT_TRUE(h.canUndo());

    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, alpha(c, 0, 0));
    ASSERT_EQ_INT(0, alpha(c, 1, 0));
    ASSERT_EQ_INT(0, alpha(c, 2, 0));
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.canRedo());

    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(255, alpha(c, 0, 0));
    ASSERT_EQ_INT(255, alpha(c, 2, 0));
    ASSERT_TRUE(!h.canRedo());
}

TEST(pixel_written_twice_restores_original)
{
    History h;
    Cel c(2, 2);
    c.set(0, 0, BLUE);
    h.beginStroke(c);
    h.setPixel(0, 0, RED);
    h.setPixel(0, 0, RED);
    h.setPixel(0, 0, BLUE);
    h.setPixel(0, 0, RED);
    h.endStroke();
    ASSERT_EQ_INT(255, c.get(0, 0).r);

    h.undo();
    ASSERT_EQ_INT(0, c.get(0, 0).r);
    ASSERT_EQ_INT(255, c.get(0, 0).b);
    h.redo();
    ASSERT_EQ_INT(255, c.get(0, 0).r);
}

TEST(multiple_strokes_undo_in_reverse_order)
{
    History h;
    Cel c(3, 1);
    for (int x = 0; x < 3; x++) {
        h.beginStroke(c);
        h.setPixel(x, 0, RED);
        h.endStroke();
    }
    h.undo();
    ASSERT_EQ_INT(255, alpha(c, 1, 0));
    ASSERT_EQ_INT(0, alpha(c, 2, 0));
    h.undo();
    ASSERT_EQ_INT(255, alpha(c, 0, 0));
    ASSERT_EQ_INT(0, alpha(c, 1, 0));
    h.undo();
    ASSERT_EQ_INT(0, alpha(c, 0, 0));
    ASSERT_TRUE(!h.undo());
}

TEST(new_stroke_discards_redo)
{
    History h;
    Cel c(3, 1);
    h.beginStroke(c);
    h.setPixel(0, 0, RED);
    h.endStroke();
    h.undo();
    ASSERT_TRUE(h.canRedo());

    h.beginStroke(c);
    h.setPixel(1, 0, RED);
    h.endStroke();
    ASSERT_TRUE(!h.canRedo());
    ASSERT_TRUE(!h.redo());
    ASSERT_EQ_INT(0, alpha(c, 0, 0));
    ASSERT_EQ_INT(255, alpha(c, 1, 0));
}

TEST(empty_and_noop_strokes_are_dropped)
{
    History h;
    Cel c(2, 2);
    h.beginStroke(c);
    ASSERT_TRUE(h.endStroke());
    ASSERT_TRUE(!h.canUndo());

    c.set(0, 0, RED);
    h.beginStroke(c);
    h.setPixel(0, 0, RED);
    h.endStroke();
    ASSERT_TRUE(!h.canUndo());
}

TEST(out_of_bounds_pixels_are_rejected)
{
    History h;
    Cel c(2, 2);
    h.beginStroke(c);
    ASSERT_TRUE(!h.setPixel(2, 0, RED));
    ASSERT_TRUE(!h.setPixel(0, -1, RED));
    h.endStroke();
    ASSERT_TRUE(!h.canUndo());
}

TEST(only_one_stroke_at_a_time)
{
    History h;
    Cel c(2, 2);
    ASSERT_TRUE(h.beginStroke(c));
    ASSERT_TRUE(!h.beginStroke(c));
    h.setPixel(0, 0, RED);
    ASSERT_TRUE(!h.undo());
    ASSERT_TRUE(!h.redo());
    ASSERT_TRUE(h.endStroke());
}

TEST(strokes_on_different_cels)
{
    History h;
    Cel a(2, 2);
    Cel b(2, 2);
    h.beginStroke(a);
    h.setPixel(0, 0, RED);
    h.endStroke();
    h.beginStroke(b);
    h.setPixel(1, 1, BLUE);
    h.endStroke();

    h.undo();
    ASSERT_EQ_INT(255, alpha(a, 0, 0));
    ASSERT_EQ_INT(0, alpha(b, 1, 1));
    h.undo();
    ASSERT_EQ_INT(0, alpha(a, 0, 0));
}

TEST(destroy_with_active_stroke_and_redo_is_clean)
{
    History h;
    Cel c(2, 2);
    h.beginStroke(c);
    h.setPixel(0, 0, RED);
    h.endStroke();
    h.undo();
    h.beginStroke(c);
    h.setPixel(1, 1, RED);
}

TEST(many_strokes_grow_storage)
{
    History h;
    Cel c(64, 64);
    for (int i = 0; i < 1000; i++) {
        h.beginStroke(c);
        h.setPixel(i % 64, (i / 64) % 64, (i & 1) ? RED : BLUE);
        h.endStroke();
    }
    for (int i = 0; i < 1000; i++) {
        ASSERT_TRUE(h.undo());
    }
    ASSERT_TRUE(!h.undo());
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            ASSERT_EQ_INT(0, alpha(c, x, y));
        }
    }
}

TEST(cancel_stroke_restores_pixels_and_keeps_history)
{
    History h;
    Cel c(4, 4);
    Rgba red = {255, 0, 0, 255}, blue = {0, 0, 255, 255};
    ASSERT_TRUE(h.beginStroke(c));
    ASSERT_TRUE(h.setPixel(1, 1, red));
    ASSERT_TRUE(h.endStroke());
    ASSERT_TRUE(h.beginStroke(c));
    ASSERT_TRUE(h.setPixel(1, 1, blue));
    ASSERT_TRUE(h.setPixel(2, 2, blue));
    ASSERT_TRUE(h.cancelStroke());
    ASSERT_EQ_INT(red.r, c.get(1, 1).r);
    ASSERT_EQ_INT(0, c.get(1, 1).b);
    ASSERT_EQ_INT(0, c.get(2, 2).a);
    ASSERT_TRUE(!h.endStroke());
    ASSERT_TRUE(!h.cancelStroke());
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(!h.canUndo());
}

TEST(forget_cel_drops_only_that_cels_steps)
{
    History h;
    Cel a(4, 4);
    Cel b(4, 4);
    Rgba red = {255, 0, 0, 255};
    h.beginStroke(a);
    h.setPixel(0, 0, red);
    h.endStroke();
    h.beginStroke(b);
    h.setPixel(1, 1, red);
    h.endStroke();
    h.beginStroke(a);
    h.setPixel(2, 2, red);
    h.endStroke();

    h.forgetCel(&a);
    ASSERT_TRUE(h.undo()); /* only b's stroke is left */
    ASSERT_EQ_INT(0, b.get(1, 1).a);
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(255, b.get(1, 1).a);
    ASSERT_TRUE(!h.canRedo());
}

TEST(forget_cel_keeps_undo_redo_boundary)
{
    History h;
    Cel a(4, 4);
    Cel b(4, 4);
    Rgba red = {255, 0, 0, 255};
    h.beginStroke(a);
    h.setPixel(0, 0, red);
    h.endStroke();
    h.beginStroke(b);
    h.setPixel(1, 1, red);
    h.endStroke();
    h.beginStroke(a);
    h.setPixel(2, 2, red);
    h.endStroke();
    h.undo(); /* a(2,2) is redoable; a(0,0) and b(1,1) are applied */

    h.forgetCel(&a);
    ASSERT_TRUE(!h.canRedo());
    ASSERT_TRUE(h.canUndo());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, b.get(1, 1).a);
}

TEST(forget_cel_cancels_active_stroke_on_it)
{
    History h;
    Cel a(4, 4);
    Rgba red = {255, 0, 0, 255};
    h.beginStroke(a);
    h.setPixel(0, 0, red);
    h.forgetCel(&a);
    ASSERT_EQ_INT(0, a.get(0, 0).a);
    ASSERT_TRUE(!h.endStroke());
    ASSERT_TRUE(h.beginStroke(a));
    h.endStroke();
}

struct Counter {
    int undos = 0, redos = 0, releases = 0;
};

/* The callables share one owner whose destruction stands for the action being released. */
static HistoryAction counter_action(Counter *counter)
{
    std::shared_ptr<Counter> token(counter, [](Counter *c) { c->releases++; });
    return HistoryAction{[token] { token->undos++; }, [token] { token->redos++; }};
}

TEST(action_undo_redo_calls_callbacks)
{
    Counter n;
    {
        History h;
        ASSERT_TRUE(h.pushAction(counter_action(&n)));
        ASSERT_TRUE(h.canUndo());
        ASSERT_TRUE(h.undo());
        ASSERT_EQ_INT(1, n.undos);
        ASSERT_TRUE(h.canRedo());
        ASSERT_TRUE(h.redo());
        ASSERT_EQ_INT(1, n.redos);
        ASSERT_EQ_INT(0, n.releases);
    }
    ASSERT_EQ_INT(1, n.releases);
}

TEST(action_interleaves_with_strokes)
{
    History h;
    Cel c(4, 4);
    Counter n;
    h.beginStroke(c);
    h.setPixel(0, 0, RED);
    h.endStroke();
    h.pushAction(counter_action(&n));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(255, alpha(c, 0, 0));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, alpha(c, 0, 0));
    ASSERT_EQ_INT(1, n.undos);
}

TEST(new_edit_releases_redone_actions)
{
    Cel c(4, 4);
    Counter n;
    {
        History h;
        h.pushAction(counter_action(&n));
        h.undo();
        h.beginStroke(c);
        h.setPixel(1, 1, BLUE);
        h.endStroke();
        ASSERT_EQ_INT(1, n.releases);
    }
    ASSERT_EQ_INT(1, n.releases);
}

TEST(action_rejected_during_stroke_is_released)
{
    History h;
    Cel c(4, 4);
    Counter n;
    h.beginStroke(c);
    ASSERT_TRUE(!h.pushAction(counter_action(&n)));
    ASSERT_EQ_INT(1, n.releases);
    h.endStroke();
}

TEST(forget_cel_keeps_actions)
{
    History h;
    Cel c(4, 4);
    Counter n;
    h.beginStroke(c);
    h.setPixel(0, 0, RED);
    h.endStroke();
    h.pushAction(counter_action(&n));
    h.forgetCel(&c);
    ASSERT_EQ_INT(0, n.releases);
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(1, n.undos);
    ASSERT_TRUE(!h.canUndo());
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
