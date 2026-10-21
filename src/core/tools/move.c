#include "tinysprite/move.h"

#include <stdlib.h>

struct MoveStroke {
    History *history;
    Cel *cel;
    int x, y, w, h; /* selection clipped to the cel */
    Rgba *pixels;   /* captured selection, w * h row-major */
};

static int clip_selection(const Cel *cel, int *x, int *y, int *w, int *h)
{
    if (*w < 1 || *h < 1) {
        return -1;
    }
    long x0 = *x < 0 ? 0 : *x;
    long y0 = *y < 0 ? 0 : *y;
    long x1 = (long)*x + *w;
    long y1 = (long)*y + *h;
    if (x1 > cel_width(cel)) {
        x1 = cel_width(cel);
    }
    if (y1 > cel_height(cel)) {
        y1 = cel_height(cel);
    }
    if (x0 >= x1 || y0 >= y1) {
        return -1;
    }
    *x = (int)x0;
    *y = (int)y0;
    *w = (int)(x1 - x0);
    *h = (int)(y1 - y0);
    return 0;
}

MoveStroke *move_stroke_begin(History *history, Cel *cel, int x, int y, int w, int h)
{
    if (!history || !cel || clip_selection(cel, &x, &y, &w, &h) != 0) {
        return NULL;
    }
    MoveStroke *stroke = calloc(1, sizeof(*stroke));
    if (!stroke) {
        return NULL;
    }
    stroke->pixels = malloc((size_t)w * (size_t)h * sizeof(*stroke->pixels));
    if (!stroke->pixels) {
        free(stroke);
        return NULL;
    }
    if (history_begin_stroke(history, cel) != 0) {
        free(stroke->pixels);
        free(stroke);
        return NULL;
    }
    stroke->history = history;
    stroke->cel = cel;
    stroke->x = x;
    stroke->y = y;
    stroke->w = w;
    stroke->h = h;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            stroke->pixels[(size_t)j * (size_t)w + (size_t)i] = cel_get(cel, x + i, y + j);
        }
    }
    return stroke;
}

int move_stroke_update(MoveStroke *stroke, int dx, int dy)
{
    if (!stroke) {
        return -1;
    }
    /* Reverting the open stroke and starting a new one redraws from the original pixels. */
    (void)history_cancel_stroke(stroke->history);
    if (history_begin_stroke(stroke->history, stroke->cel) != 0) {
        return -1;
    }
    if (dx == 0 && dy == 0) {
        return 0;
    }
    for (int j = 0; j < stroke->h; j++) {
        for (int i = 0; i < stroke->w; i++) {
            (void)history_set_pixel(stroke->history, stroke->x + i, stroke->y + j,
                                    (Rgba){0, 0, 0, 0});
        }
    }
    for (int j = 0; j < stroke->h; j++) {
        for (int i = 0; i < stroke->w; i++) {
            Rgba pixel = stroke->pixels[(size_t)j * (size_t)stroke->w + (size_t)i];
            if (pixel.a != 0) {
                /* Out-of-bounds pixels are rejected by history: they are lost. */
                (void)history_set_pixel(stroke->history, stroke->x + i + dx, stroke->y + j + dy,
                                        pixel);
            }
        }
    }
    return 0;
}

int move_stroke_end(MoveStroke *stroke)
{
    if (!stroke) {
        return -1;
    }
    int result = history_end_stroke(stroke->history);
    free(stroke->pixels);
    free(stroke);
    return result;
}

void move_stroke_cancel(MoveStroke *stroke)
{
    if (!stroke) {
        return;
    }
    (void)history_cancel_stroke(stroke->history);
    free(stroke->pixels);
    free(stroke);
}

int move_stroke(History *history, Cel *cel, int x, int y, int w, int h, int dx, int dy)
{
    MoveStroke *stroke = move_stroke_begin(history, cel, x, y, w, h);
    if (!stroke) {
        return -1;
    }
    if (move_stroke_update(stroke, dx, dy) != 0) {
        move_stroke_cancel(stroke);
        return -1;
    }
    return move_stroke_end(stroke);
}
