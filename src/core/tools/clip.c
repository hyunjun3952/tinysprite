#include "tinysprite/clip.h"

#include <stdlib.h>

struct Clip {
    int width, height;
    Rgba *pixels; /* row-major, width * height */
};

Clip *clip_copy(const Cel *cel, Rect area)
{
    if (!cel) {
        return NULL;
    }
    Rect r = rect_clip(area, cel_width(cel), cel_height(cel));
    if (rect_is_empty(r)) {
        return NULL;
    }
    Clip *clip = calloc(1, sizeof(*clip));
    if (!clip) {
        return NULL;
    }
    clip->width = r.w;
    clip->height = r.h;
    clip->pixels = malloc((size_t)clip->width * (size_t)clip->height * sizeof(*clip->pixels));
    if (!clip->pixels) {
        free(clip);
        return NULL;
    }
    for (int j = 0; j < clip->height; j++) {
        for (int i = 0; i < clip->width; i++) {
            clip->pixels[(size_t)j * (size_t)clip->width + (size_t)i] =
                cel_get(cel, r.x + i, r.y + j);
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
        return RGBA_TRANSPARENT;
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

int clip_rotate(Clip *clip, bool clockwise)
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

/* Draws the clip's opaque pixels; those landing outside the cel are skipped. Returns -1 if a
 * pixel could not be recorded. */
static int write_clip(History *history, const Cel *cel, const Clip *clip, int x, int y)
{
    int result = 0;
    for (int j = 0; j < clip->height; j++) {
        for (int i = 0; i < clip->width; i++) {
            Rgba color = clip->pixels[(size_t)j * (size_t)clip->width + (size_t)i];
            if (color.a != 0 && cel_in_bounds(cel, x + i, y + j) &&
                history_set_pixel(history, x + i, y + j, color) != 0) {
                result = -1;
            }
        }
    }
    return result;
}

/* Ends the open stroke, or abandons it if `result` says drawing it failed. */
static int finish(History *history, int result)
{
    if (result != 0) {
        (void)history_cancel_stroke(history);
        return -1;
    }
    return history_end_stroke(history);
}

int clip_paste(History *history, Cel *cel, const Clip *clip, int x, int y)
{
    if (!history || !cel || !clip || history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    return finish(history, write_clip(history, cel, clip, x, y));
}

int clip_replace(History *history, Cel *cel, Rect cleared, const Clip *clip, int x, int y)
{
    if (!history || !cel || !clip || history_begin_stroke(history, cel) != 0) {
        return -1;
    }
    Rect area = rect_clip(cleared, cel_width(cel), cel_height(cel));
    int result = 0;
    for (int j = area.y; j < area.y + area.h; j++) {
        for (int i = area.x; i < area.x + area.w; i++) {
            if (history_set_pixel(history, i, j, RGBA_TRANSPARENT) != 0) {
                result = -1;
            }
        }
    }
    if (write_clip(history, cel, clip, x, y) != 0) {
        result = -1;
    }
    return finish(history, result);
}
