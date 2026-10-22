#ifndef TINYSPRITE_UI_LAYER_THUMBNAILS_HPP
#define TINYSPRITE_UI_LAYER_THUMBNAILS_HPP

#include <SDL.h>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

#include "tinysprite/sprite.hpp"

/* One small texture per layer showing what is painted on it (not blended with the layers below,
 * and regardless of the layer's visibility). A texture is rebuilt when the document's revision
 * changes, or the size it must have does. The textures belong to the renderer given at creation,
 * so this must be destroyed before it. */
class LayerThumbnails
{
  public:
    explicit LayerThumbnails(SDL_Renderer *renderer);

    struct Thumb {
        SDL_Texture *texture = nullptr; /* null if it could not be created */
        int width = 0;
        int height = 0;
    };

    /* The thumbnail of `layer`'s cel in `frame`, fitted into maxWidth x maxHeight (see
     * thumbnail_size) and current as of `revision`. */
    Thumb get(const Layer &layer, int frame, int maxWidth, int maxHeight, uint64_t revision);

    /* Drops every thumbnail (another document is shown: layer addresses and revisions of the old
     * one mean nothing for it). */
    void clear()
    {
        graveyard_ = std::move(entries_);
        entries_.clear();
    }

    /* Drops the thumbnails of layers that are no longer in `sprite`. */
    void prune(const Sprite &sprite);

    /* How many thumbnails are kept. */
    size_t size() const
    {
        return entries_.size();
    }

  private:
    struct TextureDeleter {
        void operator()(SDL_Texture *texture) const
        {
            SDL_DestroyTexture(texture);
        }
    };
    struct Entry {
        std::unique_ptr<SDL_Texture, TextureDeleter> texture;
        int width = 0;
        int height = 0;
        int frame = -1;
        uint64_t revision = 0;
    };

    SDL_Renderer *renderer_;
    std::unordered_map<const Layer *, Entry> entries_;
    /* What clear() took out: the frame being built may have drawn these textures already, so they
     * are destroyed by the next get() or prune(), in a later frame. */
    std::unordered_map<const Layer *, Entry> graveyard_;
};

#endif
