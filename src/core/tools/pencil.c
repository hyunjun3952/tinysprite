#include "tinysprite/pencil.h"

#include <stdlib.h>

#include "tinysprite/raster.h"

typedef struct PlotTarget {
    History *history;
    Rgba color;
} PlotTarget;

static void plot_to_history(int x, int y, void *user)
{
    const PlotTarget *target = user;
    /* Out-of-bounds pixels are rejected by history; clipping is the intended behavior. */
    (void)history_set_pixel(target->history, x, y, target->color);
}

int pencil_stroke(History *history, Cel *cel, const ToolPoint *points, int count, Rgba color)
{
    if (!history || !cel || !points || count < 1) {
        return -1;
    }
    if (history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    PlotTarget target = {history, color};
    if (count == 1) {
        raster_line(points[0].x, points[0].y, points[0].x, points[0].y, plot_to_history, &target);
    }
    for (int i = 1; i < count; i++) {
        raster_line(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, plot_to_history,
                    &target);
    }
    return history_end_stroke(history);
}

int eraser_stroke(History *history, Cel *cel, const ToolPoint *points, int count)
{
    return pencil_stroke(history, cel, points, count, (Rgba){0, 0, 0, 0});
}

/* One plotted pixel and the (up to four) cels positions it touched with the colors they had before,
 * so a pixel-perfect correction can put them back. */
typedef struct Placed {
    int x, y;
    int count;
    struct {
        int x, y;
        Rgba before;
    } copies[4];
} Placed;

struct PencilStroke {
    History *history;
    Cel *cel;
    Rgba color;
    PencilOptions options;
    int last_x, last_y;
    Placed window[2]; /* pixel-perfect: the last two kept pixels, oldest first */
    int window_count;
};

static void place_pixel(PencilStroke *stroke, int x, int y, Placed *placed)
{
    int width = cel_width(stroke->cel);
    int height = cel_height(stroke->cel);
    int xs[2] = {x, width - 1 - x};
    int ys[2] = {y, height - 1 - y};
    int nx = stroke->options.mirror_x ? 2 : 1;
    int ny = stroke->options.mirror_y ? 2 : 1;

    placed->x = x;
    placed->y = y;
    placed->count = 0;
    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            int cx = xs[i];
            int cy = ys[j];
            /* A pixel on a mirror axis is its own reflection: draw it once. */
            if ((i == 1 && cx == xs[0]) || (j == 1 && cy == ys[0])) {
                continue;
            }
            if (!cel_in_bounds(stroke->cel, cx, cy)) {
                continue;
            }
            int n = placed->count++;
            placed->copies[n].x = cx;
            placed->copies[n].y = cy;
            placed->copies[n].before = cel_get(stroke->cel, cx, cy);
            (void)history_set_pixel(stroke->history, cx, cy, stroke->color);
        }
    }
}

static void unplace_pixel(PencilStroke *stroke, const Placed *placed)
{
    for (int i = 0; i < placed->count; i++) {
        (void)history_set_pixel(stroke->history, placed->copies[i].x, placed->copies[i].y,
                                placed->copies[i].before);
    }
}

int pencil_brush_size(const PencilOptions *options)
{
    if (options->size < 1) {
        return 1;
    }
    return options->size > PENCIL_BRUSH_MAX ? PENCIL_BRUSH_MAX : options->size;
}

int pencil_brush_origin(const PencilOptions *options)
{
    return (pencil_brush_size(options) - 1) / 2;
}

/* A round brush leaves out the pixels whose center lies too far from the brush center;
 * (2i - size + 1) is twice the offset from it. */
int pencil_brush_contains(const PencilOptions *options, int i, int j)
{
    int size = pencil_brush_size(options);
    if (i < 0 || j < 0 || i >= size || j >= size) {
        return 0;
    }
    if (!options->round) {
        return 1;
    }
    int dx = 2 * i - size + 1;
    int dy = 2 * j - size + 1;
    return dx * dx + dy * dy <= size * size - size;
}

/* Paints the whole brush at (x, y), mirroring each of its pixels separately so a reflected stamp
 * is the exact mirror image even for an even size. */
static void stamp_brush(PencilStroke *stroke, int size, int x, int y)
{
    int origin = pencil_brush_origin(&stroke->options);
    Placed unused;
    for (int j = 0; j < size; j++) {
        for (int i = 0; i < size; i++) {
            if (pencil_brush_contains(&stroke->options, i, j)) {
                place_pixel(stroke, x - origin + i, y - origin + j, &unused);
            }
        }
    }
}

static int is_adjacent(int x0, int y0, int x1, int y1)
{
    return abs(x0 - x1) + abs(y0 - y1) == 1;
}

static void plot_pixel(int x, int y, void *user)
{
    PencilStroke *stroke = user;
    int size = pencil_brush_size(&stroke->options);
    if (size > 1) {
        stamp_brush(stroke, size, x, y);
        return;
    }
    if (!stroke->options.pixel_perfect) {
        Placed placed;
        place_pixel(stroke, x, y, &placed);
        return;
    }

    /* Joining segments plots the shared point twice. */
    if (stroke->window_count > 0 && stroke->window[stroke->window_count - 1].x == x &&
        stroke->window[stroke->window_count - 1].y == y) {
        return;
    }
    if (stroke->window_count == 2) {
        const Placed *a = &stroke->window[0];
        const Placed *b = &stroke->window[1];
        if (is_adjacent(a->x, a->y, b->x, b->y) && is_adjacent(b->x, b->y, x, y) &&
            abs(a->x - x) == 1 && abs(a->y - y) == 1) {
            /* a-b-new is an L: drop b so a and new touch diagonally. */
            unplace_pixel(stroke, b);
            place_pixel(stroke, x, y, &stroke->window[1]);
            return;
        }
        stroke->window[0] = stroke->window[1];
        stroke->window_count = 1;
    }
    place_pixel(stroke, x, y, &stroke->window[stroke->window_count++]);
}

PencilStroke *pencil_stroke_begin(History *history, Cel *cel, Rgba color, int erase, int x, int y)
{
    return pencil_stroke_begin_ex(history, cel, color, erase, (PencilOptions){0}, x, y);
}

PencilStroke *pencil_stroke_begin_ex(History *history, Cel *cel, Rgba color, int erase,
                                     PencilOptions options, int x, int y)
{
    if (!history || !cel) {
        return NULL;
    }
    PencilStroke *stroke = calloc(1, sizeof(*stroke));
    if (!stroke) {
        return NULL;
    }
    if (history_begin_stroke(history, cel) != 0) {
        free(stroke);
        return NULL;
    }
    stroke->history = history;
    stroke->cel = cel;
    stroke->color = erase ? (Rgba){0, 0, 0, 0} : color;
    stroke->options = options;
    stroke->last_x = x;
    stroke->last_y = y;
    plot_pixel(x, y, stroke);
    return stroke;
}

int pencil_stroke_extend(PencilStroke *stroke, int x, int y)
{
    if (!stroke) {
        return -1;
    }
    raster_line(stroke->last_x, stroke->last_y, x, y, plot_pixel, stroke);
    stroke->last_x = x;
    stroke->last_y = y;
    return 0;
}

int pencil_stroke_end(PencilStroke *stroke)
{
    if (!stroke) {
        return -1;
    }
    int result = history_end_stroke(stroke->history);
    free(stroke);
    return result;
}

void pencil_stroke_cancel(PencilStroke *stroke)
{
    if (!stroke) {
        return;
    }
    (void)history_cancel_stroke(stroke->history);
    free(stroke);
}
