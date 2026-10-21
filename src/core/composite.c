#include "tinysprite/composite.h"

#include <stddef.h>

static int div255(int value)
{
    return (value + 127) / 255;
}

/* Straight-alpha "over" of `src` (alpha already scaled by layer opacity) onto `dst`. */
static Rgba blend_over(Rgba dst, Rgba src, int src_alpha)
{
    if (src_alpha == 0) {
        return dst;
    }
    /* Nothing underneath to mix with (or nothing showing through), so the source is the result. */
    if (src_alpha == 255 || dst.a == 0) {
        return (Rgba){src.r, src.g, src.b, (uint8_t)src_alpha};
    }
    int dst_weight = div255(dst.a * (255 - src_alpha));
    int out_alpha = src_alpha + dst_weight;
    return (Rgba){
        (uint8_t)((src.r * src_alpha + dst.r * dst_weight + out_alpha / 2) / out_alpha),
        (uint8_t)((src.g * src_alpha + dst.g * dst_weight + out_alpha / 2) / out_alpha),
        (uint8_t)((src.b * src_alpha + dst.b * dst_weight + out_alpha / 2) / out_alpha),
        (uint8_t)out_alpha,
    };
}

Rgba composite_over(Rgba dst, Rgba src, uint8_t opacity)
{
    return blend_over(dst, src, div255(src.a * opacity));
}

/* The layer's cel on `frame`, or NULL when the layer does not show up in the flattened image. */
static const Cel *drawn_cel(Layer *layer, int frame)
{
    return layer_visible(layer) ? layer_cel(layer, frame) : NULL;
}

Rgba composite_pixel(Sprite *sprite, int frame, int x, int y)
{
    Rgba out = RGBA_TRANSPARENT;
    if (!sprite || frame < 0 || frame >= sprite_frame_count(sprite) || x < 0 || y < 0 ||
        x >= sprite_width(sprite) || y >= sprite_height(sprite)) {
        return out;
    }
    for (int i = 0; i < sprite_layer_count(sprite); i++) {
        Layer *layer = sprite_layer(sprite, i);
        const Cel *cel = drawn_cel(layer, frame);
        if (cel) {
            out = composite_over(out, cel_get(cel, x, y), layer_opacity(layer));
        }
    }
    return out;
}

void composite_sprite(Sprite *sprite, int frame, Rect region, Rgba *out)
{
    if (!sprite || !out || frame < 0 || frame >= sprite_frame_count(sprite)) {
        return;
    }
    int width = sprite_width(sprite);
    Rect r = rect_clip(region, width, sprite_height(sprite));

    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            out[(size_t)y * (size_t)width + (size_t)x] = RGBA_TRANSPARENT;
        }
    }
    for (int i = 0; i < sprite_layer_count(sprite); i++) {
        Layer *layer = sprite_layer(sprite, i);
        const Cel *cel = drawn_cel(layer, frame);
        if (!cel) {
            continue;
        }
        uint8_t opacity = layer_opacity(layer);
        for (int y = r.y; y < r.y + r.h; y++) {
            for (int x = r.x; x < r.x + r.w; x++) {
                Rgba *dst = &out[(size_t)y * (size_t)width + (size_t)x];
                *dst = composite_over(*dst, cel_get(cel, x, y), opacity);
            }
        }
    }
}
