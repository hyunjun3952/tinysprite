#include "tinysprite/cel.h"

#include <stdlib.h>

struct Cel {
    int width;
    int height;
    Rgba *pixels;
};

Cel *cel_create(int width, int height)
{
    if (width < 1 || width > CEL_MAX_DIMENSION || height < 1 || height > CEL_MAX_DIMENSION) {
        return NULL;
    }
    Cel *cel = calloc(1, sizeof(Cel));
    if (cel == NULL) {
        return NULL;
    }
    /* All-zero Rgba is transparent black. */
    cel->pixels = calloc((size_t)width * (size_t)height, sizeof(Rgba));
    if (cel->pixels == NULL) {
        free(cel);
        return NULL;
    }
    cel->width = width;
    cel->height = height;
    return cel;
}

void cel_destroy(Cel *cel)
{
    if (cel == NULL) {
        return;
    }
    free(cel->pixels);
    free(cel);
}

int cel_width(const Cel *cel)
{
    return cel->width;
}

int cel_height(const Cel *cel)
{
    return cel->height;
}

int cel_in_bounds(const Cel *cel, int x, int y)
{
    return x >= 0 && x < cel->width && y >= 0 && y < cel->height;
}

Rgba cel_get(const Cel *cel, int x, int y)
{
    if (!cel_in_bounds(cel, x, y)) {
        return (Rgba){0, 0, 0, 0};
    }
    return cel->pixels[(size_t)y * (size_t)cel->width + (size_t)x];
}

int cel_set(Cel *cel, int x, int y, Rgba color)
{
    if (!cel_in_bounds(cel, x, y)) {
        return -1;
    }
    cel->pixels[(size_t)y * (size_t)cel->width + (size_t)x] = color;
    return 0;
}

Rgba *cel_pixels(Cel *cel)
{
    return cel->pixels;
}
