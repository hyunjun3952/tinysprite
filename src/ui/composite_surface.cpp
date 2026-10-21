#include "composite_surface.h"

#include "tinysprite/composite.h"

CompositeSurface::CompositeSurface(std::unique_ptr<SDL_Texture, TextureDeleter> texture, int width,
                                   int height)
    : texture_(std::move(texture)), width_(width), height_(height),
      pixels_(static_cast<size_t>(width) * static_cast<size_t>(height))
{
    markAllDirty();
}

std::unique_ptr<CompositeSurface> CompositeSurface::create(SDL_Renderer *renderer, int width,
                                                           int height)
{
    if (width < 1 || height < 1) {
        return nullptr;
    }
    std::unique_ptr<SDL_Texture, TextureDeleter> texture(SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height));
    if (!texture) {
        return nullptr;
    }
    SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture.get(), SDL_ScaleModeNearest);
    return std::unique_ptr<CompositeSurface>(
        new CompositeSurface(std::move(texture), width, height));
}

void CompositeSurface::markDirty(Rect area)
{
    dirtyRect_ = dirty_ ? rect_union(dirtyRect_, area) : area;
    dirty_ = true;
}

void CompositeSurface::markAllDirty()
{
    dirtyRect_ = Rect{0, 0, width_, height_};
    dirty_ = true;
}

void CompositeSurface::update(const Sprite &sprite, int frame, const OnionSkin *onion)
{
    if (!dirty_) {
        return;
    }
    dirty_ = false;
    Rect r = rect_clip(dirtyRect_, width_, height_);
    if (rect_is_empty(r)) {
        return;
    }
    if (onion) {
        composite_onion(sprite, frame, *onion, r, pixels_.data());
    } else {
        composite_sprite(sprite, frame, r, pixels_.data());
    }
    SDL_Rect rect = {r.x, r.y, r.w, r.h};
    const Rgba *origin =
        &pixels_[static_cast<size_t>(r.y) * static_cast<size_t>(width_) + static_cast<size_t>(r.x)];
    SDL_UpdateTexture(texture_.get(), &rect, origin, width_ * static_cast<int>(sizeof(Rgba)));
}

Rgba CompositeSurface::pixelAt(int x, int y) const
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return Rgba{};
    }
    return pixels_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)];
}
