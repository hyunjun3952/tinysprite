#include "tinysprite/history.h"

#include <stdlib.h>

typedef struct Delta {
    size_t index;
    Rgba old_color;
    Rgba new_color;
} Delta;

typedef struct Stroke {
    Cel *cel;
    Delta *deltas;
    size_t count;
    size_t capacity;
} Stroke;

struct History {
    Stroke *strokes; /* strokes[0..cursor) are applied, strokes[cursor..count) can be redone */
    size_t count;
    size_t capacity;
    size_t cursor;
    Stroke active;
    int stroke_open;
};

static void stroke_release(Stroke *stroke)
{
    free(stroke->deltas);
    *stroke = (Stroke){0};
}

/* Restores old colors newest-first so a pixel written several times ends at its original value. */
static void stroke_revert(const Stroke *stroke)
{
    Rgba *pixels = cel_pixels(stroke->cel);
    for (size_t i = stroke->count; i > 0; i--) {
        pixels[stroke->deltas[i - 1].index] = stroke->deltas[i - 1].old_color;
    }
}

static void stroke_apply(const Stroke *stroke)
{
    Rgba *pixels = cel_pixels(stroke->cel);
    for (size_t i = 0; i < stroke->count; i++) {
        pixels[stroke->deltas[i].index] = stroke->deltas[i].new_color;
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
        stroke_release(&history->strokes[i]);
    }
    stroke_release(&history->active);
    free(history->strokes);
    free(history);
}

int history_begin_stroke(History *history, Cel *cel)
{
    if (!history || !cel || history->stroke_open) {
        return -1;
    }
    history->active = (Stroke){.cel = cel};
    history->stroke_open = 1;
    return 0;
}

int history_set_pixel(History *history, int x, int y, Rgba color)
{
    if (!history || !history->stroke_open) {
        return -1;
    }
    Stroke *stroke = &history->active;
    if (!cel_in_bounds(stroke->cel, x, y)) {
        return -1;
    }
    size_t index = (size_t)y * (size_t)cel_width(stroke->cel) + (size_t)x;
    Rgba *pixel = &cel_pixels(stroke->cel)[index];
    if (pixel->r == color.r && pixel->g == color.g && pixel->b == color.b && pixel->a == color.a) {
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

int history_end_stroke(History *history)
{
    if (!history || !history->stroke_open) {
        return -1;
    }
    Stroke *active = &history->active;
    history->stroke_open = 0;
    if (active->count == 0) {
        stroke_release(active);
        return 0;
    }

    if (history->count == history->capacity) {
        size_t capacity = history->capacity ? history->capacity * 2 : 16;
        Stroke *strokes = realloc(history->strokes, capacity * sizeof(*strokes));
        if (!strokes) {
            stroke_revert(active);
            stroke_release(active);
            return -1;
        }
        history->strokes = strokes;
        history->capacity = capacity;
    }
    for (size_t i = history->cursor; i < history->count; i++) {
        stroke_release(&history->strokes[i]);
    }
    history->count = history->cursor;
    history->strokes[history->count++] = *active;
    history->cursor = history->count;
    *active = (Stroke){0};
    return 0;
}

int history_can_undo(const History *history)
{
    return history && history->cursor > 0;
}

int history_can_redo(const History *history)
{
    return history && history->cursor < history->count;
}

int history_undo(History *history)
{
    if (!history_can_undo(history) || history->stroke_open) {
        return -1;
    }
    stroke_revert(&history->strokes[--history->cursor]);
    return 0;
}

int history_redo(History *history)
{
    if (!history_can_redo(history) || history->stroke_open) {
        return -1;
    }
    stroke_apply(&history->strokes[history->cursor++]);
    return 0;
}
