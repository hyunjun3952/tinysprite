#ifndef TINYSPRITE_COMPOSITE_H
#define TINYSPRITE_COMPOSITE_H

#include "tinysprite/rect.h"
#include "tinysprite/sprite.h"

/* Flattens the visible layers of `frame` (bottom to top, straight-alpha "over", scaled by layer
 * opacity) into `out`, a sprite.width() x sprite.height() row-major buffer. Only the pixels inside
 * `region` (clipped to the sprite) are written, so callers can re-composite a dirty rect. Does
 * nothing if out is null or the frame is out of range. */
void composite_sprite(const Sprite &sprite, int frame, Rect region, Rgba *out);

/* The flattened color at (x, y) of `frame`, computed like composite_sprite. Returns transparent
 * black if the frame is out of range or (x, y) is out of bounds. */
Rgba composite_pixel(const Sprite &sprite, int frame, int x, int y);

/* Straight-alpha "over": `src`, its alpha scaled by `opacity` (0..255), drawn onto `dst`. This is
 * the blend composite_sprite uses for every layer. */
Rgba composite_over(Rgba dst, Rgba src, uint8_t opacity);

#endif
