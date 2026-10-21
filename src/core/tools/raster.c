#include "tinysprite/raster.h"

#include <stdlib.h>

void raster_line(int x0, int y0, int x1, int y1, RasterPlotFn plot, void *user)
{
    long long dx = llabs((long long)x1 - x0);
    long long dy = -llabs((long long)y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    long long err = dx + dy;

    for (;;) {
        plot(x0, y0, user);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        long long e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void hspan(int x0, int x1, int y, RasterPlotFn plot, void *user)
{
    for (int x = x0; x <= x1; x++) {
        plot(x, y, user);
    }
}

static void order(int *a, int *b)
{
    if (*a > *b) {
        int t = *a;
        *a = *b;
        *b = t;
    }
}

void raster_rect(int x0, int y0, int x1, int y1, int filled, RasterPlotFn plot, void *user)
{
    order(&x0, &x1);
    order(&y0, &y1);

    if (filled) {
        for (int y = y0; y <= y1; y++) {
            hspan(x0, x1, y, plot, user);
        }
        return;
    }

    hspan(x0, x1, y0, plot, user);
    if (y1 > y0) {
        hspan(x0, x1, y1, plot, user);
    }
    for (int y = y0 + 1; y < y1; y++) {
        plot(x0, y, user);
        if (x1 > x0) {
            plot(x1, y, user);
        }
    }
}

/* Midpoint ellipse over the bounding box, after Alois Zingl's "rasterizing ellipses in a
 * rectangle". Walks one quadrant and mirrors it into the other three. */
void raster_ellipse(int x0, int y0, int x1, int y1, int filled, RasterPlotFn plot, void *user)
{
    order(&x0, &x1);
    order(&y0, &y1);

    long long a = (long long)x1 - x0;
    long long b = (long long)y1 - y0;
    long long b1 = b & 1;
    long long dx = 4 * (1 - a) * b * b;
    long long dy = 4 * (b1 + 1) * a * a;
    long long err = dx + dy + b1 * a * a;

    long long lx = x0;
    long long rx = x1;
    long long dn = y0 + (b + 1) / 2;
    long long up = dn - b1;
    int new_row = 1;
    long long step_a = 8 * a * a;
    long long step_b = 8 * b * b;

    do {
        if (filled) {
            if (new_row) {
                hspan((int)lx, (int)rx, (int)dn, plot, user);
                if (up != dn) {
                    hspan((int)lx, (int)rx, (int)up, plot, user);
                }
            }
        } else {
            plot((int)rx, (int)dn, user);
            plot((int)lx, (int)dn, user);
            if (up != dn) {
                plot((int)lx, (int)up, user);
                plot((int)rx, (int)up, user);
            }
        }
        new_row = 0;

        long long e2 = 2 * err;
        if (e2 <= dy) {
            dn++;
            up--;
            dy += step_a;
            err += dy;
            new_row = 1;
        }
        if (e2 >= dx || 2 * err > dy) {
            lx++;
            rx--;
            dx += step_b;
            err += dx;
        }
    } while (lx <= rx);

    /* Flat ellipses sdn early: finish the remaining rows at the tips. */
    while (dn - up <= b) {
        if (filled) {
            hspan((int)lx - 1, (int)rx + 1, (int)dn, plot, user);
            hspan((int)lx - 1, (int)rx + 1, (int)up, plot, user);
        } else {
            plot((int)lx - 1, (int)dn, user);
            plot((int)rx + 1, (int)dn, user);
            plot((int)lx - 1, (int)up, user);
            plot((int)rx + 1, (int)up, user);
        }
        dn++;
        up--;
    }
}

typedef struct Seed {
    int x, y;
} Seed;

typedef struct SeedStack {
    Seed *items;
    size_t count, capacity;
} SeedStack;

static int stack_push(SeedStack *s, int x, int y)
{
    if (s->count == s->capacity) {
        size_t capacity = s->capacity ? s->capacity * 2 : 64;
        Seed *items = realloc(s->items, capacity * sizeof(*items));
        if (!items) {
            return -1;
        }
        s->items = items;
        s->capacity = capacity;
    }
    s->items[s->count++] = (Seed){x, y};
    return 0;
}

static int same_color(Rgba a, Rgba b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

/* Queues one seed per run of fillable pixels in row y, between x0 and x1. */
static int push_runs(SeedStack *s, const Cel *cel, const unsigned char *seen, Rgba target, int x0,
                     int x1, int y)
{
    if (y < 0 || y >= cel_height(cel)) {
        return 0;
    }
    int w = cel_width(cel);
    int in_run = 0;
    for (int x = x0; x <= x1; x++) {
        int fillable = !seen[(size_t)y * w + x] && same_color(cel_get(cel, x, y), target);
        if (fillable && !in_run && stack_push(s, x, y) != 0) {
            return -1;
        }
        in_run = fillable;
    }
    return 0;
}

int raster_flood_fill(const Cel *cel, int x, int y, RasterPlotFn plot, void *user)
{
    if (!cel_in_bounds(cel, x, y)) {
        return -1;
    }
    int w = cel_width(cel);
    int h = cel_height(cel);
    Rgba target = cel_get(cel, x, y);
    unsigned char *seen = calloc((size_t)w * h, 1);
    SeedStack stack = {0};
    int result = -1;

    if (!seen || stack_push(&stack, x, y) != 0) {
        goto done;
    }
    while (stack.count > 0) {
        Seed seed = stack.items[--stack.count];
        if (seen[(size_t)seed.y * w + seed.x]) {
            continue;
        }
        int left = seed.x;
        int right = seed.x;
        while (left > 0 && !seen[(size_t)seed.y * w + left - 1] &&
               same_color(cel_get(cel, left - 1, seed.y), target)) {
            left--;
        }
        while (right < w - 1 && !seen[(size_t)seed.y * w + right + 1] &&
               same_color(cel_get(cel, right + 1, seed.y), target)) {
            right++;
        }
        for (int i = left; i <= right; i++) {
            seen[(size_t)seed.y * w + i] = 1;
        }
        if (push_runs(&stack, cel, seen, target, left, right, seed.y - 1) != 0 ||
            push_runs(&stack, cel, seen, target, left, right, seed.y + 1) != 0) {
            goto done;
        }
    }

    /* Report only once the region is known, so the callback can edit the cel it came from. */
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            if (seen[(size_t)row * w + col]) {
                plot(col, row, user);
            }
        }
    }
    result = 0;

done:
    free(stack.items);
    free(seen);
    return result;
}
