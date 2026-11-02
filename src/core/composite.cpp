#include "tinysprite/composite.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <vector>

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

/* Calls visit(x, y) for every cell of the rectangle, row by row. */
template <class Visit> void for_each_cell(const Rect &r, Visit visit)
{
    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            visit(x, y);
        }
    }
}

/* The layer's cel on `frame`, or null when the layer does not show up in the flattened image. */
const Cel *drawnCel(const Layer &layer, int frame)
{
    return layer.visible() ? layer.cel(frame) : nullptr;
}

/*
 * Flattens `frame` over the cels of `r`, which lies inside the sprite, into `out`: its first pixel
 * is the top-left of `r` and its rows are `stride` pixels apart. It walks the cels' rows directly,
 * because every cel of a sprite has the sprite's size.
 */
void flatten(const Sprite &sprite, int frame, const Rect &r, Rgba *out, size_t stride)
{
    for (int y = 0; y < r.h; y++) {
        std::fill_n(out + static_cast<size_t>(y) * stride, r.w, Rgba{});
    }
    for (int i = 0; i < sprite.layerCount(); i++) {
        const Layer &layer = *sprite.layer(i);
        const Cel *cel = drawnCel(layer, frame);
        if (!cel) {
            continue;
        }
        for (int y = 0; y < r.h; y++) {
            const Rgba *from = cel->pixels() + cel->indexOf(r.x, r.y + y);
            Rgba *to = out + static_cast<size_t>(y) * stride;
            for (int x = 0; x < r.w; x++) {
                to[x] = composite_blend(to[x], from[x], layer.opacity(), layer.blendMode());
            }
        }
    }
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
    const auto width = static_cast<size_t>(sprite.width());
    const Rect r = rect_clip(region, sprite.width(), sprite.height());
    flatten(sprite, frame, r, out + static_cast<size_t>(r.y) * width + static_cast<size_t>(r.x),
            width);
}

namespace
{

/* How strongly the ghost `distance` frames away (1 = nearest) of `count` shows. */
uint8_t ghostOpacity(const OnionSkin &skin, int distance, int count)
{
    return static_cast<uint8_t>(skin.opacity * (count - distance + 1) / count);
}

/* A pixel of a ghost: the flattened one mixed with the tint. */
Rgba ghostPixel(Rgba p, Rgba tint)
{
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

/*
 * The region of a composite_onion: where its first pixel is and how its rows lie in `out`, and a
 * buffer of the region's size for one flattened frame.
 */
struct OnionRegion {
    Rect rect;
    Rgba *out;
    size_t stride;
    std::vector<Rgba> flat;
};

/* Puts `frame`, each of its pixels made by `pixel` from the flattened one, over the region. */
template <class Pixel>
void overRegion(OnionRegion &region, const Sprite &sprite, int frame, uint8_t opacity, Pixel pixel)
{
    const Rect &r = region.rect;
    const auto width = static_cast<size_t>(r.w);
    flatten(sprite, frame, r, region.flat.data(), width);
    for (int y = 0; y < r.h; y++) {
        const Rgba *from = region.flat.data() + static_cast<size_t>(y) * width;
        Rgba *to = region.out + static_cast<size_t>(y) * region.stride;
        for (int x = 0; x < r.w; x++) {
            to[x] = composite_over(to[x], pixel(from[x]), opacity);
        }
    }
}

/* The ghosts of the frame, far to near so the nearest ends up on top. */
void drawGhosts(OnionRegion &region, const Sprite &sprite, int frame, const OnionPlan &plan)
{
    auto tinted = [](Rgba tint) { return [tint](Rgba p) { return ghostPixel(p, tint); }; };
    for (int d = std::max(plan.before, plan.after); d >= 1; d--) {
        if (d <= plan.after) {
            overRegion(region, sprite, frame + d, ghostOpacity(plan.skin, d, plan.wantAfter),
                       tinted(ONION_FUTURE_TINT));
        }
        if (d <= plan.before) {
            overRegion(region, sprite, frame - d, ghostOpacity(plan.skin, d, plan.wantBefore),
                       tinted(ONION_PAST_TINT));
        }
    }
}

} // namespace

namespace
{

/* The pixel of `map` at sprite position (x, y), the map's origin taken into account; transparent
 * outside the map, over an empty cell, or one that names a tile the tileset does not have. */
Rgba tilemapPixel(const Tileset &tileset, const TileMap &map, int x, int y)
{
    const int mapX = x - map.x;
    const int mapY = y - map.y;
    if (mapX < 0 || mapY < 0) {
        return Rgba{};
    }
    return tile_pixel(tileset, map.cell(mapX / tileset.tileWidth, mapY / tileset.tileHeight),
                      mapX % tileset.tileWidth, mapY % tileset.tileHeight);
}

/* Whether the map can be drawn: it is shown, has one cell per grid position and a usable tileset.
 */
bool drawableMap(const Sprite &sprite, const TileMap &map)
{
    return map.visible && map.tileset >= 0 &&
           map.tileset < static_cast<int>(sprite.tilesets().size()) && map.columns > 0 &&
           map.rows > 0 &&
           map.cells.size() == static_cast<size_t>(map.columns) * static_cast<size_t>(map.rows) &&
           tileset_valid(sprite.tilesets()[static_cast<size_t>(map.tileset)]);
}

} // namespace

void composite_tilemaps(const Sprite &sprite, Rect region, Rgba *out)
{
    if (!out) {
        return;
    }
    auto width = static_cast<size_t>(sprite.width());
    Rect r = rect_clip(region, sprite.width(), sprite.height());
    for (const TileMap &map : sprite.tilemaps()) {
        if (!drawableMap(sprite, map)) {
            continue;
        }
        const Tileset &tileset = sprite.tilesets()[static_cast<size_t>(map.tileset)];
        for_each_cell(r, [&](int x, int y) {
            Rgba &dst = out[static_cast<size_t>(y) * width + static_cast<size_t>(x)];
            dst = composite_over(dst, tilemapPixel(tileset, map, x, y), 255);
        });
    }
}

void composite_onion(const Sprite &sprite, int frame, const OnionSkin &skin, Rect region, Rgba *out)
{
    if (!out || frame < 0 || frame >= sprite.frameCount()) {
        return;
    }
    const auto width = static_cast<size_t>(sprite.width());
    const Rect r = rect_clip(region, sprite.width(), sprite.height());
    /* Ghosts beyond the animation do not exist, so the count is what can actually be drawn. */
    const OnionPlan plan{skin, std::clamp(skin.before, 0, frame),
                         std::clamp(skin.after, 0, sprite.frameCount() - 1 - frame),
                         std::max(skin.before, 1), std::max(skin.after, 1)};
    OnionRegion area{r, out + static_cast<size_t>(r.y) * width + static_cast<size_t>(r.x), width,
                     std::vector<Rgba>(static_cast<size_t>(r.w) * static_cast<size_t>(r.h))};
    for (int y = 0; y < r.h; y++) {
        std::fill_n(area.out + static_cast<size_t>(y) * width, r.w, Rgba{});
    }
    drawGhosts(area, sprite, frame, plan);
    overRegion(area, sprite, frame, 255, [](Rgba p) { return p; });
}
