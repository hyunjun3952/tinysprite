#include "tinysprite/palette.h"

#include <stdlib.h>

struct Palette {
    Rgba colors[PALETTE_MAX_COLORS];
    int count;
};

Palette *palette_create(void)
{
    return calloc(1, sizeof(Palette));
}

void palette_destroy(Palette *palette)
{
    free(palette);
}

int palette_count(const Palette *palette)
{
    return palette->count;
}

int palette_add(Palette *palette, Rgba color)
{
    if (palette->count >= PALETTE_MAX_COLORS) {
        return -1;
    }
    palette->colors[palette->count] = color;
    return palette->count++;
}

Rgba palette_get(const Palette *palette, int index)
{
    if (index < 0 || index >= palette->count) {
        return (Rgba){0, 0, 0, 0};
    }
    return palette->colors[index];
}

int palette_set(Palette *palette, int index, Rgba color)
{
    if (index < 0 || index >= palette->count) {
        return -1;
    }
    palette->colors[index] = color;
    return 0;
}

int palette_remove(Palette *palette, int index)
{
    if (index < 0 || index >= palette->count) {
        return -1;
    }
    for (int i = index; i < palette->count - 1; i++) {
        palette->colors[i] = palette->colors[i + 1];
    }
    palette->colors[--palette->count] = (Rgba){0, 0, 0, 0};
    return 0;
}

int palette_find(const Palette *palette, Rgba color)
{
    for (int i = 0; i < palette->count; i++) {
        const Rgba *c = &palette->colors[i];
        if (c->r == color.r && c->g == color.g && c->b == color.b && c->a == color.a) {
            return i;
        }
    }
    return -1;
}
