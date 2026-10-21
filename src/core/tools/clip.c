#include "tinysprite/clip.h"

#include <stdlib.h>

struct Clip {
    int width, height;
    Rgba *pixels; /* row-major, width * height */
};

Clip *clip_copy(const Cel *cel, int x, int y, int w, int h)
{
    if (!cel) {
        return NULL;
    }
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > cel_width(cel) ? cel_width(cel) : x + w;
    int y1 = y + h > cel_height(cel) ? cel_height(cel) : y + h;
    if (w < 1 || h < 1 || x0 >= x1 || y0 >= y1) {
        return NULL;
    }
    Clip *clip = calloc(1, sizeof(*clip));
    if (!clip) {
        return NULL;
    }
    clip->width = x1 - x0;
    clip->height = y1 - y0;
    clip->pixels = malloc((size_t)clip->width * (size_t)clip->height * sizeof(*clip->pixels));
    if (!clip->pixels) {
        free(clip);
        return NULL;
    }
    for (int j = 0; j < clip->height; j++) {
        for (int i = 0; i < clip->width; i++) {
            clip->pixels[(size_t)j * (size_t)clip->width + (size_t)i] =
                cel_get(cel, x0 + i, y0 + j);
        }
    }
    return clip;
}

void clip_destroy(Clip *clip)
{
    if (clip) {
        free(clip->pixels);
        free(clip);
    }
}

int clip_width(const Clip *clip)
{
    return clip ? clip->width : 0;
}

int clip_height(const Clip *clip)
{
    return clip ? clip->height : 0;
}

Rgba clip_get(const Clip *clip, int x, int y)
{
    if (!clip || x < 0 || y < 0 || x >= clip->width || y >= clip->height) {
        return (Rgba){0, 0, 0, 0};
    }
    return clip->pixels[(size_t)y * (size_t)clip->width + (size_t)x];
}

void clip_flip_horizontal(Clip *clip)
{
    if (!clip) {
        return;
    }
    for (int y = 0; y < clip->height; y++) {
        Rgba *row = &clip->pixels[(size_t)y * (size_t)clip->width];
        for (int x = 0; x < clip->width / 2; x++) {
            Rgba t = row[x];
            row[x] = row[clip->width - 1 - x];
            row[clip->width - 1 - x] = t;
        }
    }
}

void clip_flip_vertical(Clip *clip)
{
    if (!clip) {
        return;
    }
    for (int y = 0; y < clip->height / 2; y++) {
        Rgba *top = &clip->pixels[(size_t)y * (size_t)clip->width];
        Rgba *bottom = &clip->pixels[(size_t)(clip->height - 1 - y) * (size_t)clip->width];
        for (int x = 0; x < clip->width; x++) {
            Rgba t = top[x];
            top[x] = bottom[x];
            bottom[x] = t;
        }
    }
}

int clip_rotate(Clip *clip, int clockwise)
{
    if (!clip) {
        return -1;
    }
    Rgba *turned = malloc((size_t)clip->width * (size_t)clip->height * sizeof(*turned));
    if (!turned) {
        return -1;
    }
    /* The turned clip is height x width; each of its pixels comes from one source pixel. */
    int new_width = clip->height;
    int new_height = clip->width;
    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            int sx = clockwise ? y : clip->width - 1 - y;
            int sy = clockwise ? clip->height - 1 - x : x;
            turned[(size_t)y * (size_t)new_width + (size_t)x] =
                clip->pixels[(size_t)sy * (size_t)clip->width + (size_t)sx];
        }
    }
    free(clip->pixels);
    clip->pixels = turned;
    clip->width = new_width;
    clip->height = new_height;
    return 0;
}

static void write_clip(History *history, const Cel *cel, const Clip *clip, int x, int y)
{
    for (int j = 0; j < clip->height; j++) {
        for (int i = 0; i < clip->width; i++) {
            Rgba color = clip->pixels[(size_t)j * (size_t)clip->width + (size_t)i];
            /* Out-of-bounds pixels are rejected by history, which is the clipping we want. */
            if (color.a != 0 && cel_in_bounds(cel, x + i, y + j)) {
                (void)history_set_pixel(history, x + i, y + j, color);
            }
        }
    }
}

int clip_paste(History *history, Cel *cel, const Clip *clip, int x, int y)
{
    if (!history || !cel || !clip || history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    write_clip(history, cel, clip, x, y);
    return history_end_stroke(history);
}

int clip_replace(History *history, Cel *cel, int x, int y, int w, int h, const Clip *clip, int nx,
                 int ny)
{
    if (!history || !cel || !clip || history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            if (cel_in_bounds(cel, i, j)) {
                (void)history_set_pixel(history, i, j, (Rgba){0, 0, 0, 0});
            }
        }
    }
    write_clip(history, cel, clip, nx, ny);
    return history_end_stroke(history);
}
