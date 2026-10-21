#include "tinysprite/composite.h"

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
