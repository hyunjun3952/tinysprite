#ifndef TINYSPRITE_UI_COMPOSITE_SURFACE_HPP
#define TINYSPRITE_UI_COMPOSITE_SURFACE_HPP

#include <SDL3/SDL.h>

#include <memory>
#include <vector>

#include "tinysprite/composite.hpp"
#include "tinysprite/palette.hpp"
#include "tinysprite/rect.hpp"
#include "tinysprite/sprite.hpp"

/* The flattened frame the canvas shows: a CPU buffer plus the streaming texture it is uploaded to
 * (nearest sampling, blended). Changes are reported as dirty rectangles and update() composites
 * and uploads only the union of them. */
class CompositeSurface
{
  public:
    /* A surface that starts fully dirty. Null if the size is invalid or the texture cannot be
     * created. */
    static std::unique_ptr<CompositeSurface> create(SDL_Renderer *renderer, int width, int height);

    SDL_Texture *texture() const
    {
        return texture_.get();
    }

    /* Marks an area for re-compositing; it is clipped to the surface when it is used. */
    void markDirty(Rect area);
    void markAllDirty();
    bool dirty() const
    {
        return dirty_;
    }
    Rect dirtyRect() const
    {
        return dirtyRect_;
    }

    /* Composites the dirty area of `frame` of the sprite (which must have the surface's size) and
     * uploads it; with `onion` the neighbouring frames are drawn faintly under it. */
    void update(const Sprite &sprite, int frame, const OnionSkin *onion = nullptr);

    /* The last composited color at (x, y). */
    Rgba pixelAt(int x, int y) const;

  private:
    struct TextureDeleter {
        void operator()(SDL_Texture *texture) const
        {
            SDL_DestroyTexture(texture);
        }
    };

    CompositeSurface(std::unique_ptr<SDL_Texture, TextureDeleter> texture, int width, int height);

    std::unique_ptr<SDL_Texture, TextureDeleter> texture_;
    int width_;
    int height_;
    std::vector<Rgba> pixels_;
    bool dirty_ = false;
    Rect dirtyRect_ = {};
};

#endif
