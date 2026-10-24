#include "cel_texture.hpp"

bool CelTexture::set(SDL_Renderer *renderer, const Cel &cel)
{
    if (!texture_ || width_ != cel.width() || height_ != cel.height()) {
        release();
        texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                     cel.width(), cel.height());
        if (!texture_) {
            return false;
        }
        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
        width_ = cel.width();
        height_ = cel.height();
    }
    return SDL_UpdateTexture(texture_, nullptr, cel.pixels(),
                             cel.width() * static_cast<int>(sizeof(Rgba)));
}

void CelTexture::release()
{
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    width_ = 0;
    height_ = 0;
}
