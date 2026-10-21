#include "tinysprite/history.h"

#include <stdlib.h>

typedef struct Delta {
    size_t index;
    Rgba old_color;
    Rgba new_color;
} Delta;

typedef enum StepKind {
    STEP_PIXELS, /* deltas on `cel` */
    STEP_ACTION, /* `action` */
} StepKind;

/* One undo step. */
typedef struct Step {
    StepKind kind;
    Cel *cel;
    HistoryAction action;
    Delta *deltas;
    size_t count;
    size_t capacity;
} Step;

struct History {
    Step *steps; /* steps[0..cursor) are applied, steps[cursor..count) can be redone */
    size_t count;
    size_t capacity;
    size_t cursor;
    Step active; /* the open stroke; it is open exactly while active.cel is set */
};

static void step_release(Step *step)
{
    if (step->kind == STEP_ACTION && step->action.release) {
        step->action.release(step->action.ctx);
    }
    free(step->deltas);
    *step = (Step){0};
}

/* Restores old colors newest-first so a pixel written several times ends at its original value. */
static void step_revert(const Step *step)
{
    if (step->kind == STEP_ACTION) {
        if (step->action.undo) {
            step->action.undo(step->action.ctx);
        }
        return;
    }
    Rgba *pixels = cel_pixels(step->cel);
    for (size_t i = step->count; i > 0; i--) {
        pixels[step->deltas[i - 1].index] = step->deltas[i - 1].old_color;
    }
}

static void step_apply(const Step *step)
{
    if (step->kind == STEP_ACTION) {
        if (step->action.redo) {
            step->action.redo(step->action.ctx);
        }
        return;
    }
    Rgba *pixels = cel_pixels(step->cel);
    for (size_t i = 0; i < step->count; i++) {
        pixels[step->deltas[i].index] = step->deltas[i].new_color;
    }
}

History *history_create(void)
{
    return calloc(1, sizeof(History));
}

void history_destroy(History *history)
{
    if (!history) {
        return;
    }
    for (size_t i = 0; i < history->count; i++) {
        step_release(&history->steps[i]);
    }
    step_release(&history->active);
    free(history->steps);
    free(history);
}

bool history_stroke_active(const History *history)
{
    return history && history->active.cel;
}

int history_begin_stroke(History *history, Cel *cel)
{
    if (!history || !cel || history_stroke_active(history)) {
        return -1;
    }
    history->active = (Step){.kind = STEP_PIXELS, .cel = cel};
    return 0;
}

int history_set_pixel(History *history, int x, int y, Rgba color)
{
    if (!history_stroke_active(history)) {
        return -1;
    }
    Step *stroke = &history->active;
    if (!cel_in_bounds(stroke->cel, x, y)) {
        return -1;
    }
    size_t index = (size_t)y * (size_t)cel_width(stroke->cel) + (size_t)x;
    Rgba *pixel = &cel_pixels(stroke->cel)[index];
    if (rgba_equal(*pixel, color)) {
        return 0;
    }
    if (stroke->count == stroke->capacity) {
        size_t capacity = stroke->capacity ? stroke->capacity * 2 : 32;
        Delta *deltas = realloc(stroke->deltas, capacity * sizeof(*deltas));
        if (!deltas) {
            return -1;
        }
        stroke->deltas = deltas;
        stroke->capacity = capacity;
    }
    stroke->deltas[stroke->count++] = (Delta){index, *pixel, color};
    *pixel = color;
    return 0;
}

/* Appends `step` after the cursor, discarding the redo steps. On success the history owns the
 * step; on allocation failure nothing changes. */
static int history_commit(History *history, const Step *step)
{
    if (history->count == history->capacity) {
        size_t capacity = history->capacity ? history->capacity * 2 : 16;
        Step *steps = realloc(history->steps, capacity * sizeof(*steps));
        if (!steps) {
            return -1;
        }
        history->steps = steps;
        history->capacity = capacity;
    }
    for (size_t i = history->cursor; i < history->count; i++) {
        step_release(&history->steps[i]);
    }
    history->count = history->cursor;
    history->steps[history->count++] = *step;
    history->cursor = history->count;
    return 0;
}

int history_end_stroke(History *history)
{
    if (!history_stroke_active(history)) {
        return -1;
    }
    Step stroke = history->active;
    history->active = (Step){0};
    if (stroke.count == 0) {
        step_release(&stroke);
        return 0;
    }
    if (history_commit(history, &stroke) != 0) {
        step_revert(&stroke);
        step_release(&stroke);
        return -1;
    }
    return 0;
}

int history_cancel_stroke(History *history)
{
    if (!history_stroke_active(history)) {
        return -1;
    }
    Step stroke = history->active;
    history->active = (Step){0};
    step_revert(&stroke);
    step_release(&stroke);
    return 0;
}

int history_push_action(History *history, HistoryAction action)
{
    Step step = {.kind = STEP_ACTION, .action = action};
    if (!history || history_stroke_active(history) || history_commit(history, &step) != 0) {
        step_release(&step);
        return -1;
    }
    return 0;
}

bool history_can_undo(const History *history)
{
    return history && history->cursor > 0;
}

bool history_can_redo(const History *history)
{
    return history && history->cursor < history->count;
}

int history_undo(History *history)
{
    if (!history_can_undo(history) || history_stroke_active(history)) {
        return -1;
    }
    step_revert(&history->steps[--history->cursor]);
    return 0;
}

int history_redo(History *history)
{
    if (!history_can_redo(history) || history_stroke_active(history)) {
        return -1;
    }
    step_apply(&history->steps[history->cursor++]);
    return 0;
}

void history_forget_cel(History *history, Cel *cel)
{
    if (!history || !cel) {
        return;
    }
    if (history->active.cel == cel) {
        (void)history_cancel_stroke(history);
    }
    size_t kept = 0;
    size_t kept_applied = 0;
    for (size_t i = 0; i < history->count; i++) {
        if (history->steps[i].kind == STEP_PIXELS && history->steps[i].cel == cel) {
            step_release(&history->steps[i]);
            continue;
        }
        history->steps[kept++] = history->steps[i];
        if (i < history->cursor) {
            kept_applied++;
        }
    }
    history->count = kept;
    history->cursor = kept_applied;
}
