#include "tinysprite/composite.h"

#include <algorithm>
#include <cstddef>

namespace
{

int div255(int value)
{
    return (value + 127) / 255;
}

/* Straight-alpha "over" of `src` (alpha already scaled by layer opacity) onto `dst`. */
Rgba blendOver(Rgba dst, Rgba src, int srcAlpha)
{
    if (srcAlpha == 0) {
        return dst;
    }
    /* Nothing underneath to mix with (or nothing showing through), so the source is the result. */
    if (srcAlpha == 255 || dst.a == 0) {
        return Rgba{src.r, src.g, src.b, static_cast<uint8_t>(srcAlpha)};
    }
    int dstWeight = div255(dst.a * (255 - srcAlpha));
    int outAlpha = srcAlpha + dstWeight;
    auto mix = [&](int s, int d) {
        return static_cast<uint8_t>((s * srcAlpha + d * dstWeight + outAlpha / 2) / outAlpha);
    };
    return Rgba{mix(src.r, dst.r), mix(src.g, dst.g), mix(src.b, dst.b),
                static_cast<uint8_t>(outAlpha)};
}

/* The layer's cel on `frame`, or null when the layer does not show up in the flattened image. */
const Cel *drawnCel(const Layer &layer, int frame)
{
    return layer.visible() ? layer.cel(frame) : nullptr;
}

} // namespace

Rgba composite_over(Rgba dst, Rgba src, uint8_t opacity)
{
    return blendOver(dst, src, div255(src.a * opacity));
}

Rgba composite_pixel(const Sprite &sprite, int frame, int x, int y)
{
    Rgba out;
    if (frame < 0 || frame >= sprite.frameCount() || x < 0 || y < 0 || x >= sprite.width() ||
        y >= sprite.height()) {
        return out;
    }
    for (int i = 0; i < sprite.layerCount(); i++) {
        const Layer &layer = *sprite.layer(i);
        if (const Cel *cel = drawnCel(layer, frame)) {
            out = composite_over(out, cel->get(x, y), layer.opacity());
        }
    }
    return out;
}

void composite_sprite(const Sprite &sprite, int frame, Rect region, Rgba *out)
{
    if (!out || frame < 0 || frame >= sprite.frameCount()) {
        return;
    }
    size_t width = static_cast<size_t>(sprite.width());
    Rect r = rect_clip(region, sprite.width(), sprite.height());

    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            out[static_cast<size_t>(y) * width + static_cast<size_t>(x)] = Rgba{};
        }
    }
    for (int i = 0; i < sprite.layerCount(); i++) {
        const Layer &layer = *sprite.layer(i);
        const Cel *cel = drawnCel(layer, frame);
        if (!cel) {
            continue;
        }
        uint8_t opacity = layer.opacity();
        for (int y = r.y; y < r.y + r.h; y++) {
            for (int x = r.x; x < r.x + r.w; x++) {
                Rgba &dst = out[static_cast<size_t>(y) * width + static_cast<size_t>(x)];
                dst = composite_over(dst, cel->get(x, y), opacity);
            }
        }
    }
}

namespace
{

/* How strongly the ghost `distance` frames away (1 = nearest) of `count` shows. */
uint8_t ghostOpacity(const OnionSkin &skin, int distance, int count)
{
    return static_cast<uint8_t>(skin.opacity * (count - distance + 1) / count);
}

/* The ghost of `frame` at (x, y): its flattened pixel mixed with the tint. */
Rgba ghostPixel(const Sprite &sprite, int frame, int x, int y, Rgba tint)
{
    Rgba p = composite_pixel(sprite, frame, x, y);
    auto half = [](int a, int b) { return static_cast<uint8_t>((a + b + 1) / 2); };
    return Rgba{half(p.r, tint.r), half(p.g, tint.g), half(p.b, tint.b), p.a};
}

} // namespace

void composite_onion(const Sprite &sprite, int frame, const OnionSkin &skin, Rect region, Rgba *out)
{
    if (!out || frame < 0 || frame >= sprite.frameCount()) {
        return;
    }
    size_t width = static_cast<size_t>(sprite.width());
    Rect r = rect_clip(region, sprite.width(), sprite.height());
    /* Ghosts beyond the animation do not exist, so the count is what can actually be drawn. */
    int before = std::clamp(skin.before, 0, frame);
    int after = std::clamp(skin.after, 0, sprite.frameCount() - 1 - frame);
    int wantBefore = std::max(skin.before, 1);
    int wantAfter = std::max(skin.after, 1);

    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            Rgba acc;
            /* far to near, so the nearest ghost ends up on top */
            int far = std::max(before, after);
            for (int d = far; d >= 1; d--) {
                if (d <= after) {
                    acc =
                        composite_over(acc, ghostPixel(sprite, frame + d, x, y, ONION_FUTURE_TINT),
                                       ghostOpacity(skin, d, wantAfter));
                }
                if (d <= before) {
                    acc = composite_over(acc, ghostPixel(sprite, frame - d, x, y, ONION_PAST_TINT),
                                         ghostOpacity(skin, d, wantBefore));
                }
            }
            out[static_cast<size_t>(y) * width + static_cast<size_t>(x)] =
                composite_over(acc, composite_pixel(sprite, frame, x, y), 255);
        }
    }
}
