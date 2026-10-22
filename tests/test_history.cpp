#include <memory>
#include <vector>

#include "minitest.hpp"
#include "tinysprite/history.hpp"

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
        h.setPixel(i % 64, (i / 64) % 64, (i & 1) != 0 ? RED : BLUE);
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

TEST(forget_cel_keeps_redo_steps_of_other_cels)
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
    h.beginStroke(b);
    h.setPixel(2, 2, red);
    h.endStroke();
    h.undo(); /* b(2,2) is redoable; a(0,0) and b(1,1) are applied */

    h.forgetCel(&a);
    ASSERT_TRUE(h.canRedo());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(255, b.get(2, 2).a);
    ASSERT_TRUE(!h.canRedo());
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, b.get(1, 1).a);
    ASSERT_TRUE(!h.canUndo());
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

TEST(perform_runs_redo_and_records_the_step)
{
    Counter n;
    {
        History h;
        ASSERT_TRUE(h.perform(counter_action(&n)));
        ASSERT_EQ_INT(1, n.redos);
        ASSERT_TRUE(h.undo());
        ASSERT_EQ_INT(1, n.undos);
        ASSERT_TRUE(h.redo());
        ASSERT_EQ_INT(2, n.redos);
    }
    ASSERT_EQ_INT(1, n.releases);
}

TEST(perform_refused_during_stroke_runs_nothing)
{
    History h;
    Cel c(4, 4);
    Counter n;
    h.beginStroke(c);
    ASSERT_TRUE(!h.perform(counter_action(&n)));
    ASSERT_EQ_INT(0, n.redos);
    ASSERT_EQ_INT(1, n.releases);
    h.endStroke();
}

TEST(perform_accepts_an_action_without_redo)
{
    History h;
    HistoryAction action;
    action.undo = [] {};
    ASSERT_TRUE(h.perform(std::move(action)));
    ASSERT_TRUE(h.canUndo());
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

static void paint(History &h, Cel &c, int x)
{
    ASSERT_TRUE(h.beginStroke(c));
    ASSERT_TRUE(h.setPixel(x, 0, RED));
    ASSERT_TRUE(h.endStroke());
}

TEST(clip_rejects_pixels_outside_the_rect)
{
    History h;
    Cel c(4, 4);
    h.setClip(Rect{1, 1, 2, 2});
    ASSERT_TRUE(h.beginStroke(c));
    ASSERT_TRUE(h.setPixel(1, 1, RED));
    ASSERT_TRUE(!h.setPixel(0, 0, RED));
    ASSERT_TRUE(!h.setPixel(3, 1, RED));
    ASSERT_TRUE(h.setPixel(2, 2, RED));
    ASSERT_TRUE(h.endStroke());
    ASSERT_EQ_INT(0, alpha(c, 0, 0));
    ASSERT_EQ_INT(0, alpha(c, 3, 1));
    ASSERT_EQ_INT(255, alpha(c, 1, 1));
    ASSERT_EQ_INT(255, alpha(c, 2, 2));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, alpha(c, 1, 1));
}

TEST(clearing_the_clip_allows_every_pixel_again)
{
    History h;
    Cel c(4, 4);
    h.setClip(Rect{1, 1, 1, 1});
    h.setClip(std::nullopt);
    ASSERT_TRUE(h.beginStroke(c));
    ASSERT_TRUE(h.setPixel(0, 0, RED));
    ASSERT_TRUE(h.endStroke());
}

TEST(save_point_starts_clean)
{
    History h;
    ASSERT_TRUE(h.atSavePoint());
}

TEST(save_point_follows_edits_undo_and_redo)
{
    History h;
    Cel c(4, 4);
    paint(h, c, 0);
    ASSERT_TRUE(!h.atSavePoint());
    h.markSaved();
    ASSERT_TRUE(h.atSavePoint());
    paint(h, c, 1);
    ASSERT_TRUE(!h.atSavePoint());
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(h.atSavePoint());
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(!h.atSavePoint());
    ASSERT_TRUE(h.redo());
    ASSERT_TRUE(h.atSavePoint());
}

TEST(save_point_is_lost_when_a_new_edit_discards_it)
{
    History h;
    Cel c(4, 4);
    paint(h, c, 0);
    paint(h, c, 1);
    h.markSaved();
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(h.undo());
    paint(h, c, 2);
    paint(h, c, 3);
    /* Two steps are applied again, as when saved, but they are not the saved ones. */
    ASSERT_TRUE(!h.atSavePoint());
}

TEST(save_point_is_lost_when_forgetting_cels)
{
    History h;
    Cel a(4, 4);
    Cel b(4, 4);
    paint(h, a, 0);
    paint(h, b, 0);
    h.markSaved();
    h.forgetCel(&a);
    ASSERT_TRUE(!h.atSavePoint());
}

/* An action that logs -n when undone and +n when redone, so the order of a group shows. */
static HistoryAction logging_action(std::vector<int> *log, int n)
{
    return HistoryAction{[log, n] { log->push_back(-n); }, [log, n] { log->push_back(n); }};
}

TEST(group_is_one_undo_step_that_runs_its_actions_in_reverse)
{
    History h;
    std::vector<int> log;
    ASSERT_TRUE(h.beginGroup());
    h.pushAction(logging_action(&log, 1));
    h.pushAction(logging_action(&log, 2));
    h.pushAction(logging_action(&log, 3));
    ASSERT_TRUE(h.endGroup());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(3, static_cast<int>(log.size()));
    ASSERT_EQ_INT(-3, log[0]);
    ASSERT_EQ_INT(-2, log[1]);
    ASSERT_EQ_INT(-1, log[2]);
    ASSERT_TRUE(!h.canUndo());
    log.clear();
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(1, log[0]);
    ASSERT_EQ_INT(2, log[1]);
    ASSERT_EQ_INT(3, log[2]);
    ASSERT_TRUE(!h.canRedo());
}

TEST(group_mixes_with_strokes_and_other_steps)
{
    History h;
    Cel c(4, 4);
    std::vector<int> log;
    paint(h, c, 0);
    h.beginGroup();
    h.pushAction(logging_action(&log, 1));
    h.pushAction(logging_action(&log, 2));
    h.endGroup();
    paint(h, c, 1);
    ASSERT_TRUE(h.undo()); /* the stroke */
    ASSERT_TRUE(log.empty());
    ASSERT_TRUE(h.undo()); /* the whole group */
    ASSERT_EQ_INT(2, static_cast<int>(log.size()));
    ASSERT_EQ_INT(255, alpha(c, 0, 0));
    ASSERT_TRUE(h.undo()); /* the first stroke */
    ASSERT_EQ_INT(0, alpha(c, 0, 0));
}

TEST(an_empty_group_records_nothing_and_keeps_redo)
{
    History h;
    Cel c(4, 4);
    paint(h, c, 0);
    h.undo();
    ASSERT_TRUE(h.beginGroup());
    ASSERT_TRUE(h.endGroup());
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.canRedo());
}

TEST(group_rules)
{
    History h;
    Cel c(4, 4);
    std::vector<int> log;
    ASSERT_TRUE(!h.endGroup());
    ASSERT_TRUE(h.beginGroup());
    ASSERT_TRUE(!h.beginGroup());   /* no nesting */
    ASSERT_TRUE(!h.beginStroke(c)); /* strokes are not part of a group */
    h.pushAction(logging_action(&log, 1));
    ASSERT_TRUE(!h.undo()); /* nothing to undo until it ends */
    ASSERT_TRUE(h.endGroup());
    ASSERT_TRUE(h.beginStroke(c));
    ASSERT_TRUE(!h.beginGroup()); /* not during a stroke either */
    h.endStroke();
}

TEST(group_discards_redo_like_any_edit_and_releases_its_actions)
{
    Counter n;
    {
        History h;
        Cel c(4, 4);
        h.beginGroup();
        h.pushAction(counter_action(&n));
        h.pushAction(counter_action(&n));
        h.endGroup();
        h.undo();
        ASSERT_EQ_INT(0, n.releases);
        paint(h, c, 0); /* a new edit drops the undone group */
        ASSERT_EQ_INT(2, n.releases);
    }
    ASSERT_EQ_INT(2, n.releases);
}

TEST(group_counts_as_one_step_for_the_save_point)
{
    History h;
    std::vector<int> log;
    h.markSaved();
    h.beginGroup();
    h.pushAction(logging_action(&log, 1));
    h.pushAction(logging_action(&log, 2));
    h.endGroup();
    ASSERT_TRUE(!h.atSavePoint());
    h.undo();
    ASSERT_TRUE(h.atSavePoint());
}

TEST(a_stroke_can_carry_an_action_that_runs_with_its_undo_and_redo)
{
    Cel cel(4, 4);
    History h;
    int ran = 0;
    int pixelWhenUndone = -1;
    int pixelWhenRedone = -1;
    h.beginStroke(cel);
    h.setPixel(1, 1, RED);
    HistoryAction also;
    also.undo = [&] {
        ran += 1;
        pixelWhenUndone = cel.get(1, 1).a;
    };
    also.redo = [&] {
        ran += 10;
        pixelWhenRedone = cel.get(1, 1).a;
    };
    ASSERT_TRUE(h.endStroke(std::move(also)));
    ASSERT_EQ_INT(0, ran);
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(1, ran);
    ASSERT_EQ_INT(0, pixelWhenUndone); /* the pixels are back first */
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(11, ran);
    ASSERT_EQ_INT(255, pixelWhenRedone); /* ... and redone first */
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(!h.canUndo()); /* one step, not two */
}

TEST(a_stroke_that_changed_nothing_still_records_the_action)
{
    Cel cel(4, 4);
    History h;
    int state = 0;
    h.beginStroke(cel);
    HistoryAction also;
    also.undo = [&] { state = 0; };
    also.redo = [&] { state = 1; };
    ASSERT_TRUE(h.endStroke(also));
    state = 1;
    ASSERT_TRUE(h.canUndo());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, state);
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(1, state);
}

TEST(a_stroke_that_changed_nothing_and_has_no_action_is_dropped_as_before)
{
    Cel cel(4, 4);
    History h;
    h.beginStroke(cel);
    ASSERT_TRUE(h.endStroke(HistoryAction{}));
    ASSERT_TRUE(!h.canUndo());
}

TEST(the_action_of_a_stroke_is_released_with_its_step)
{
    Cel cel(4, 4);
    auto token = std::make_shared<int>(0);
    {
        History h;
        h.beginStroke(cel);
        h.setPixel(0, 0, RED);
        HistoryAction also;
        also.undo = [token] {};
        ASSERT_TRUE(h.endStroke(std::move(also)));
        ASSERT_EQ_INT(2, static_cast<int>(token.use_count()));
    }
    ASSERT_EQ_INT(1, static_cast<int>(token.use_count()));
}

TEST(ending_without_a_stroke_fails_and_drops_the_action)
{
    History h;
    auto token = std::make_shared<int>(0);
    HistoryAction also;
    also.undo = [token] {};
    ASSERT_TRUE(!h.endStroke(std::move(also)));
    ASSERT_EQ_INT(1, static_cast<int>(token.use_count()));
}

int main()
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
    RUN(forget_cel_keeps_redo_steps_of_other_cels);
    RUN(forget_cel_cancels_active_stroke_on_it);
    RUN(action_undo_redo_calls_callbacks);
    RUN(perform_runs_redo_and_records_the_step);
    RUN(perform_refused_during_stroke_runs_nothing);
    RUN(perform_accepts_an_action_without_redo);
    RUN(action_interleaves_with_strokes);
    RUN(new_edit_releases_redone_actions);
    RUN(action_rejected_during_stroke_is_released);
    RUN(forget_cel_keeps_actions);
    RUN(clip_rejects_pixels_outside_the_rect);
    RUN(clearing_the_clip_allows_every_pixel_again);
    RUN(group_is_one_undo_step_that_runs_its_actions_in_reverse);
    RUN(group_mixes_with_strokes_and_other_steps);
    RUN(an_empty_group_records_nothing_and_keeps_redo);
    RUN(group_rules);
    RUN(group_discards_redo_like_any_edit_and_releases_its_actions);
    RUN(group_counts_as_one_step_for_the_save_point);
    RUN(a_stroke_can_carry_an_action_that_runs_with_its_undo_and_redo);
    RUN(a_stroke_that_changed_nothing_still_records_the_action);
    RUN(a_stroke_that_changed_nothing_and_has_no_action_is_dropped_as_before);
    RUN(the_action_of_a_stroke_is_released_with_its_step);
    RUN(ending_without_a_stroke_fails_and_drops_the_action);
    RUN(save_point_starts_clean);
    RUN(save_point_follows_edits_undo_and_redo);
    RUN(save_point_is_lost_when_a_new_edit_discards_it);
    RUN(save_point_is_lost_when_forgetting_cels);
    return TEST_SUMMARY();
}
