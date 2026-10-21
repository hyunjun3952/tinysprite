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

Rgba composite_pixel(Sprite *sprite, int frame, int x, int y)
{
    Rgba out = {0, 0, 0, 0};
    if (!sprite || frame < 0 || frame >= sprite_frame_count(sprite) || x < 0 || y < 0 ||
        x >= sprite_width(sprite) || y >= sprite_height(sprite)) {
        return out;
    }
    for (int i = 0; i < sprite_layer_count(sprite); i++) {
        Layer *layer = sprite_layer(sprite, i);
        Cel *cel = layer_cel(layer, frame);
        if (!layer_visible(layer) || !cel) {
            continue;
        }
        Rgba src = cel_get(cel, x, y);
        out = blend_over(out, src, div255(src.a * layer_opacity(layer)));
    }
    return out;
}

void composite_sprite(Sprite *sprite, int frame, CompositeRect region, Rgba *out)
{
    if (!sprite || !out || frame < 0 || frame >= sprite_frame_count(sprite)) {
        return;
    }
    int width = sprite_width(sprite);
    int x0 = region.x < 0 ? 0 : region.x;
    int y0 = region.y < 0 ? 0 : region.y;
    int x1 = region.x + region.w > width ? width : region.x + region.w;
    int y1 =
        region.y + region.h > sprite_height(sprite) ? sprite_height(sprite) : region.y + region.h;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            out[(size_t)y * (size_t)width + (size_t)x] = (Rgba){0, 0, 0, 0};
        }
    }
    for (int i = 0; i < sprite_layer_count(sprite); i++) {
        Layer *layer = sprite_layer(sprite, i);
        Cel *cel = layer_cel(layer, frame);
        if (!layer_visible(layer) || !cel) {
            continue;
        }
        int opacity = layer_opacity(layer);
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                Rgba src = cel_get(cel, x, y);
                Rgba *dst = &out[(size_t)y * (size_t)width + (size_t)x];
                *dst = blend_over(*dst, src, div255(src.a * opacity));
            }
        }
    }
}
