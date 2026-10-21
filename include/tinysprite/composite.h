#ifndef TINYSPRITE_COMPOSITE_H
#define TINYSPRITE_COMPOSITE_H

#include "tinysprite/sprite.h"

typedef struct CompositeRect {
    int x, y, w, h;
} CompositeRect;

/* Flattens the visible layers of `frame` (bottom to top, straight-alpha "over", scaled by layer
 * opacity) into `out`, a sprite_width x sprite_height row-major buffer. Only the pixels inside
 * `region` (clipped to the sprite) are written, so callers can re-composite a dirty rect. Does
 * nothing if sprite or out is NULL or the frame is out of range. */
void composite_sprite(Sprite *sprite, int frame, CompositeRect region, Rgba *out);

#endif
