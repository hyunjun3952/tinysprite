#include "tinysprite/composite.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

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

/* B(Cb, Cs) for one 0..255 channel. */
int blendChannel(BlendMode mode, int cb, int cs)
{
    switch (mode) {
    case BlendMode::Multiply:
        return div255(cb * cs);
    case BlendMode::Screen:
        return cb + cs - div255(cb * cs);
    case BlendMode::Overlay:
        /* hard light with the roles swapped: the backdrop decides */
        return cb < 128 ? div255(2 * cb * cs) : 255 - div255(2 * (255 - cb) * (255 - cs));
    case BlendMode::Darken:
        return std::min(cb, cs);
    case BlendMode::Lighten:
        return std::max(cb, cs);
    case BlendMode::Add:
        return std::min(255, cb + cs);
    case BlendMode::Difference:
        return std::abs(cb - cs);
    case BlendMode::Normal:
        break;
    }
    return cs;
}

/* The layer's cel on `frame`, or null when the layer does not show up in the flattened image. */
/* Calls visit(x, y) for every cell of the rectangle, row by row. */
template <class Visit> void for_each_cell(const Rect &r, Visit visit)
{
    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            visit(x, y);
        }
    }
}

const Cel *drawnCel(const Layer &layer, int frame)
{
    return layer.visible() ? layer.cel(frame) : nullptr;
}

} // namespace

Rgba composite_over(Rgba dst, Rgba src, uint8_t opacity)
{
    return blendOver(dst, src, div255(src.a * opacity));
}

Rgba composite_blend(Rgba dst, Rgba src, uint8_t opacity, BlendMode mode)
{
    int srcAlpha = div255(src.a * opacity);
    if (mode == BlendMode::Normal || srcAlpha == 0 || dst.a == 0) {
        return blendOver(dst, src, srcAlpha);
    }
    auto mixed = [&](int cb, int cs) {
        return static_cast<uint8_t>(
            div255((255 - dst.a) * cs + dst.a * blendChannel(mode, cb, cs)));
    };
    Rgba color{mixed(dst.r, src.r), mixed(dst.g, src.g), mixed(dst.b, src.b), src.a};
    return blendOver(dst, color, srcAlpha);
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
            out = composite_blend(out, cel->get(x, y), layer.opacity(), layer.blendMode());
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
    auto dstAt = [&](int x, int y) -> Rgba & {
        return out[static_cast<size_t>(y) * width + static_cast<size_t>(x)];
    };

    for_each_cell(r, [&](int x, int y) { dstAt(x, y) = Rgba{}; });
    for (int i = 0; i < sprite.layerCount(); i++) {
        const Layer &layer = *sprite.layer(i);
        const Cel *cel = drawnCel(layer, frame);
        if (!cel) {
            continue;
        }
        for_each_cell(r, [&](int x, int y) {
            Rgba &dst = dstAt(x, y);
            dst = composite_blend(dst, cel->get(x, y), layer.opacity(), layer.blendMode());
        });
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

/* How many ghosts really show on each side of a frame, and how many the fade is spread over. */
struct OnionPlan {
    OnionSkin skin;
    int before;
    int after;
    int wantBefore;
    int wantAfter;
};

/* The ghosts under the frame at (x, y), far to near so the nearest ends up on top. */
Rgba ghostsAt(const Sprite &sprite, int frame, int x, int y, const OnionPlan &plan)
{
    Rgba acc;
    for (int d = std::max(plan.before, plan.after); d >= 1; d--) {
        if (d <= plan.after) {
            acc = composite_over(acc, ghostPixel(sprite, frame + d, x, y, ONION_FUTURE_TINT),
                                 ghostOpacity(plan.skin, d, plan.wantAfter));
        }
        if (d <= plan.before) {
            acc = composite_over(acc, ghostPixel(sprite, frame - d, x, y, ONION_PAST_TINT),
                                 ghostOpacity(plan.skin, d, plan.wantBefore));
        }
    }
    return acc;
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
    OnionPlan plan{skin, std::clamp(skin.before, 0, frame),
                   std::clamp(skin.after, 0, sprite.frameCount() - 1 - frame),
                   std::max(skin.before, 1), std::max(skin.after, 1)};

    for_each_cell(r, [&](int x, int y) {
        out[static_cast<size_t>(y) * width + static_cast<size_t>(x)] = composite_over(
            ghostsAt(sprite, frame, x, y, plan), composite_pixel(sprite, frame, x, y), 255);
    });
}
