#include "layer_thumbnails.hpp"

#include <vector>

#include "tinysprite/thumbnail.hpp"

LayerThumbnails::LayerThumbnails(SDL_Renderer *renderer) : renderer_(renderer)
{
}

LayerThumbnails::Thumb LayerThumbnails::get(const Layer &layer, int frame, int maxWidth,
                                            int maxHeight, uint64_t revision)
{
    graveyard_.clear();
    const Cel *cel = layer.cel(frame);
    if (!cel) {
        return {};
    }
    int width = 0;
    int height = 0;
    thumbnail_size(cel->width(), cel->height(), maxWidth, maxHeight, width, height);
    Entry &entry = entries_[&layer];
    if (entry.texture && entry.width == width && entry.height == height && entry.frame == frame &&
        entry.revision == revision) {
        return {entry.texture.get(), width, height};
    }
    if (!entry.texture || entry.width != width || entry.height != height) {
        entry.texture.reset(SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                              SDL_TEXTUREACCESS_STATIC, width, height));
        if (!entry.texture) {
            return {};
        }
        SDL_SetTextureBlendMode(entry.texture.get(), SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(entry.texture.get(), SDL_ScaleModeNearest);
        entry.width = width;
        entry.height = height;
    }
    std::vector<Rgba> pixels = thumbnail_pixels(*cel, width, height);
    SDL_UpdateTexture(entry.texture.get(), nullptr, pixels.data(),
                      width * static_cast<int>(sizeof(Rgba)));
    entry.frame = frame;
    entry.revision = revision;
    return {entry.texture.get(), width, height};
}

void LayerThumbnails::prune(const Sprite &sprite)
{
    for (auto it = entries_.begin(); it != entries_.end();) {
        bool present = false;
        for (int i = 0; i < sprite.layerCount() && !present; i++) {
            present = sprite.layer(i) == it->first;
        }
        it = present ? std::next(it) : entries_.erase(it);
    }
}
