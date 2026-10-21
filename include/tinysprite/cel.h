#ifndef TINYSPRITE_CEL_H
#define TINYSPRITE_CEL_H

#include "tinysprite/palette.h"

#define CEL_MAX_DIMENSION 4096

typedef struct Cel Cel;

/* Creates a fully transparent width x height RGBA buffer.
 * Returns NULL if a dimension is outside 1..CEL_MAX_DIMENSION or on allocation failure. */
Cel *cel_create(int width, int height);
void cel_destroy(Cel *cel);

int cel_width(const Cel *cel);
int cel_height(const Cel *cel);

int cel_in_bounds(const Cel *cel, int x, int y);

/* Returns transparent black for out-of-bounds coordinates. */
Rgba cel_get(const Cel *cel, int x, int y);

/* Returns 0 on success, -1 if (x, y) is out of bounds. */
int cel_set(Cel *cel, int x, int y, Rgba color);

/* Row-major pixel buffer of width * height entries, owned by the cel. */
Rgba *cel_pixels(Cel *cel);

#endif
