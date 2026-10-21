#ifndef TINYSPRITE_PALETTE_H
#define TINYSPRITE_PALETTE_H

#include <stdbool.h>
#include <stdint.h>

#define PALETTE_MAX_COLORS 256

typedef struct Rgba {
    uint8_t r, g, b, a;
} Rgba;

#define RGBA_TRANSPARENT ((Rgba){0, 0, 0, 0})

static inline bool rgba_equal(Rgba a, Rgba b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

typedef struct Palette Palette;

/* Creates an empty palette. Returns NULL on allocation failure. */
Palette *palette_create(void);
void palette_destroy(Palette *palette);

int palette_count(const Palette *palette);

/* Appends a color and returns its index, or -1 if the palette is full. */
int palette_add(Palette *palette, Rgba color);

/* Returns transparent black for an out-of-range index. */
Rgba palette_get(const Palette *palette, int index);

/* Returns 0 on success, -1 if index is out of range. */
int palette_set(Palette *palette, int index, Rgba color);

/* Removes the color at `index`; the colors after it shift down. Returns 0 on success, -1 if index
 * is out of range. */
int palette_remove(Palette *palette, int index);

/* Returns the index of the first color equal to `color` (all four channels), or -1. */
int palette_find(const Palette *palette, Rgba color);

#endif
