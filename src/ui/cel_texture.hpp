#ifndef TINYSPRITE_UI_CEL_TEXTURE_HPP
#define TINYSPRITE_UI_CEL_TEXTURE_HPP

#include <SDL3/SDL.h>

#include "tinysprite/cel.hpp"

/* A picture of a cel for ImGui to draw: a texture (nearest sampling, blended) that is made again
 * when the size changes and uploaded on every set. Owned by whoever draws it; it must be released
 * (or destroyed) before the renderer it was made with. */
class CelTexture
{
  public:
    CelTexture() = default;
    CelTexture(const CelTexture &) = delete;
    CelTexture &operator=(const CelTexture &) = delete;
    CelTexture(CelTexture &&) = delete;
    CelTexture &operator=(CelTexture &&) = delete;
    ~CelTexture()
    {
        release();
    }

    /* Shows `cel`. Returns false (and shows nothing) if the texture cannot be made. */
    bool set(SDL_Renderer *renderer, const Cel &cel);

    SDL_Texture *texture() const
    {
        return texture_;
    }
    int width() const
    {
        return width_;
    }
    int height() const
    {
        return height_;
    }

    void release();

  private:
    SDL_Texture *texture_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

#endif
