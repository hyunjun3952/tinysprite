#ifndef TINYSPRITE_COMPOSITE_HPP
#define TINYSPRITE_COMPOSITE_HPP

#include "tinysprite/layer.hpp"
#include "tinysprite/rect.hpp"
#include "tinysprite/sprite.hpp"

/* Flattens the visible layers of `frame` (bottom to top, each blended with its layer's blend mode
 * and scaled by layer opacity) into `out`, a sprite.width() x sprite.height() row-major buffer.
 * Only the pixels inside `region` (clipped to the sprite) are written, so callers can re-composite
 * a dirty rect. Does nothing if out is null or the frame is out of range. */
void composite_sprite(const Sprite &sprite, int frame, Rect region, Rgba *out);

/* The flattened color at (x, y) of `frame`, computed like composite_sprite. Returns transparent
 * black if the frame is out of range or (x, y) is out of bounds. */
Rgba composite_pixel(const Sprite &sprite, int frame, int x, int y);

/* Onion skin: the neighbours of the current frame drawn faintly under it, so the next drawing can
 * be lined up with the last one. `before` previous and `after` following frames are shown (fewer at
 * the ends of the animation), each as its flattened image mixed half and half with a tint (past
 * frames red, future frames blue). `opacity` (0..255) is how strongly the nearest ghost shows; the
 * ones further away fade out linearly. */
inline constexpr int ONION_MAX_FRAMES = 8; /* ghosts per side the UI allows */

struct OnionSkin {
    int before = 1;
    int after = 1;
    uint8_t opacity = 96;
};
inline constexpr Rgba ONION_PAST_TINT = {230, 60, 60, 255};
inline constexpr Rgba ONION_FUTURE_TINT = {60, 120, 230, 255};

/* Like composite_sprite, but with the onion skin ghosts of `frame` below it: the current frame is
 * drawn over them, so they show through its transparent and translucent pixels. Does nothing if out
 * is null or the frame is out of range. */
void composite_onion(const Sprite &sprite, int frame, const OnionSkin &skin, Rect region,
                     Rgba *out);

/* Straight-alpha "over": `src`, its alpha scaled by `opacity` (0..255), drawn onto `dst`. This is
 * how the onion skin ghosts are drawn, and BlendMode::Normal. */
Rgba composite_over(Rgba dst, Rgba src, uint8_t opacity);

/* Like composite_over, but the source color is first mixed with `mode` applied to the backdrop,
 * weighted by the backdrop's alpha (W3C compositing: Cs' = (1 - ab) * Cs + ab * B(Cb, Cs)). Where
 * the backdrop is transparent the source shows unchanged, and Normal equals composite_over. This
 * is what composite_sprite uses for every layer. */
Rgba composite_blend(Rgba dst, Rgba src, uint8_t opacity, BlendMode mode);

#endif
