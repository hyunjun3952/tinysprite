#include "tinysprite/move.h"

#include <stdlib.h>

#include "tinysprite/clip.h"

struct MoveStroke {
    History *history;
    Cel *cel;
    Rect area;    /* selection clipped to the cel */
    Clip *pixels; /* the selection as it was when the move began */
    bool failed;
};

/* Pixels outside the cel are lost, which is how a selection dragged off the edge gets clipped. */
static void put_pixel(MoveStroke *stroke, int x, int y, Rgba color)
{
    if (cel_in_bounds(stroke->cel, x, y) && history_set_pixel(stroke->history, x, y, color) != 0) {
        stroke->failed = true;
    }
}

MoveStroke *move_stroke_begin(History *history, Cel *cel, Rect selection)
{
    if (!history || !cel) {
        return NULL;
    }
    Rect area = rect_clip(selection, cel_width(cel), cel_height(cel));
    if (rect_is_empty(area)) {
        return NULL;
    }
    MoveStroke *stroke = calloc(1, sizeof(*stroke));
    if (!stroke) {
        return NULL;
    }
    stroke->pixels = clip_copy(cel, area);
    if (!stroke->pixels) {
        free(stroke);
        return NULL;
    }
    if (history_begin_stroke(history, cel) != 0) {
        clip_destroy(stroke->pixels);
        free(stroke);
        return NULL;
    }
    stroke->history = history;
    stroke->cel = cel;
    stroke->area = area;
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
    stroke->failed = false;
    if (dx == 0 && dy == 0) {
        return 0;
    }
    Rect area = stroke->area;
    for (int j = 0; j < area.h; j++) {
        for (int i = 0; i < area.w; i++) {
            put_pixel(stroke, area.x + i, area.y + j, RGBA_TRANSPARENT);
        }
    }
    for (int j = 0; j < area.h; j++) {
        for (int i = 0; i < area.w; i++) {
            Rgba pixel = clip_get(stroke->pixels, i, j);
            if (pixel.a != 0) {
                put_pixel(stroke, area.x + i + dx, area.y + j + dy, pixel);
            }
        }
    }
    return stroke->failed ? -1 : 0;
}

int move_stroke_end(MoveStroke *stroke)
{
    if (!stroke) {
        return -1;
    }
    int result = -1;
    if (stroke->failed) {
        (void)history_cancel_stroke(stroke->history);
    } else {
        result = history_end_stroke(stroke->history);
    }
    clip_destroy(stroke->pixels);
    free(stroke);
    return result;
}

void move_stroke_cancel(MoveStroke *stroke)
{
    if (!stroke) {
        return;
    }
    (void)history_cancel_stroke(stroke->history);
    clip_destroy(stroke->pixels);
    free(stroke);
}

int move_stroke(History *history, Cel *cel, Rect selection, int dx, int dy)
{
    MoveStroke *stroke = move_stroke_begin(history, cel, selection);
    if (!stroke) {
        return -1;
    }
    if (move_stroke_update(stroke, dx, dy) != 0) {
        move_stroke_cancel(stroke);
        return -1;
    }
    return move_stroke_end(stroke);
}
