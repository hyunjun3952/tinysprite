#include "tinysprite/pencil.h"

#include <stdlib.h>

#include "history_plot.h"
#include "tinysprite/raster.h"

int pencil_stroke(History *history, Cel *cel, const ToolPoint *points, int count, Rgba color)
{
    if (!history || !cel || !points || count < 1) {
        return -1;
    }
    if (history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    HistoryPlot plot = {history, cel, color, false};
    if (count == 1) {
        raster_line(points[0].x, points[0].y, points[0].x, points[0].y, history_plot, &plot);
    }
    for (int i = 1; i < count; i++) {
        raster_line(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, history_plot,
                    &plot);
    }
    if (plot.failed) {
        (void)history_cancel_stroke(history);
        return -1;
    }
    return history_end_stroke(history);
}

int eraser_stroke(History *history, Cel *cel, const ToolPoint *points, int count)
{
    return pencil_stroke(history, cel, points, count, RGBA_TRANSPARENT);
}

/* One plotted pixel and the (up to four) cel positions it touched with the colors they had
 * before, so a pixel-perfect correction can put them back. */
typedef struct Placed {
    int x, y;
    int count;
    struct {
        ToolPoint at;
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
    bool failed; /* an allocation failed; the stroke is abandoned when it ends */
};

static void write_pixel(PencilStroke *stroke, ToolPoint at, Rgba color)
{
    if (history_set_pixel(stroke->history, at.x, at.y, color) != 0) {
        stroke->failed = true;
    }
}

/* Paints (x, y) and its reflections. `placed` (may be NULL) receives what was touched. */
static void place_pixel(PencilStroke *stroke, int x, int y, Placed *placed)
{
    Placed scratch;
    if (!placed) {
        placed = &scratch;
    }
    ToolPoint points[4];
    int count = symmetry_points(stroke->options.symmetry, cel_width(stroke->cel),
                                cel_height(stroke->cel), (ToolPoint){x, y}, points);
    placed->x = x;
    placed->y = y;
    placed->count = 0;
    for (int i = 0; i < count; i++) {
        if (!cel_in_bounds(stroke->cel, points[i].x, points[i].y)) {
            continue;
        }
        placed->copies[placed->count].at = points[i];
        placed->copies[placed->count].before = cel_get(stroke->cel, points[i].x, points[i].y);
        placed->count++;
        write_pixel(stroke, points[i], stroke->color);
    }
}

/* The history records the restore as one more delta, so undoing the stroke still ends at the
 * colors from before it. */
static void unplace_pixel(PencilStroke *stroke, const Placed *placed)
{
    for (int i = 0; i < placed->count; i++) {
        write_pixel(stroke, placed->copies[i].at, placed->copies[i].before);
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

/* A round brush leaves out the pixels whose center lies too far from the brush center:
 * (2i - size + 1) is twice the offset from it, and size * size - size is the squared distance
 * (times four) that trims the corners off while keeping the edge midpoints, so size 3 becomes a
 * plus and larger sizes get rounded corners. */
static bool brush_covers(int size, bool round, int i, int j)
{
    if (i < 0 || j < 0 || i >= size || j >= size) {
        return false;
    }
    if (!round) {
        return true;
    }
    int dx = 2 * i - size + 1;
    int dy = 2 * j - size + 1;
    return dx * dx + dy * dy <= size * size - size;
}

bool pencil_brush_contains(const PencilOptions *options, int i, int j)
{
    return brush_covers(pencil_brush_size(options), options->round, i, j);
}

/* Paints the whole brush at (x, y), mirroring each of its pixels separately so a reflected stamp
 * is the exact mirror image even for an even size. */
static void stamp_brush(PencilStroke *stroke, int size, int x, int y)
{
    int origin = pencil_brush_origin(&stroke->options);
    for (int j = 0; j < size; j++) {
        for (int i = 0; i < size; i++) {
            if (brush_covers(size, stroke->options.round, i, j)) {
                place_pixel(stroke, x - origin + i, y - origin + j, NULL);
            }
        }
    }
}

static bool is_adjacent(int x0, int y0, int x1, int y1)
{
    return abs(x0 - x1) + abs(y0 - y1) == 1;
}

/* a, b and the new point (x, y) form an L: a-b and b-new are steps along different axes. */
static bool makes_corner(const Placed *a, const Placed *b, int x, int y)
{
    return is_adjacent(a->x, a->y, b->x, b->y) && is_adjacent(b->x, b->y, x, y) &&
           abs(a->x - x) == 1 && abs(a->y - y) == 1;
}

static void plot_pixel_perfect(PencilStroke *stroke, int x, int y)
{
    Placed *window = stroke->window;
    /* Joining segments plots the shared point twice. */
    if (stroke->window_count > 0 && window[stroke->window_count - 1].x == x &&
        window[stroke->window_count - 1].y == y) {
        return;
    }
    if (stroke->window_count == 2) {
        if (makes_corner(&window[0], &window[1], x, y)) {
            /* Drop the middle pixel so the outer two touch diagonally. */
            unplace_pixel(stroke, &window[1]);
            place_pixel(stroke, x, y, &window[1]);
            return;
        }
        window[0] = window[1];
        stroke->window_count = 1;
    }
    place_pixel(stroke, x, y, &window[stroke->window_count]);
    stroke->window_count++;
}

static void plot_pixel(int x, int y, void *user)
{
    PencilStroke *stroke = user;
    int size = pencil_brush_size(&stroke->options);
    if (size > 1) {
        stamp_brush(stroke, size, x, y);
    } else if (stroke->options.pixel_perfect) {
        plot_pixel_perfect(stroke, x, y);
    } else {
        place_pixel(stroke, x, y, NULL);
    }
}

PencilStroke *pencil_stroke_begin(History *history, Cel *cel, Rgba color, PencilOptions options,
                                  int x, int y)
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
    stroke->color = color;
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
    return stroke->failed ? -1 : 0;
}

int pencil_stroke_end(PencilStroke *stroke)
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
