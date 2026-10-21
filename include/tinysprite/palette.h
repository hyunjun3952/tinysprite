#ifndef TINYSPRITE_PALETTE_H
#define TINYSPRITE_PALETTE_H

#include <stdint.h>

#define PALETTE_MAX_COLORS 256

typedef struct Rgba {
    uint8_t r, g, b, a;
} Rgba;

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

#endif
