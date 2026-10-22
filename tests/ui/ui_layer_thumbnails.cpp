/* The per-layer thumbnail cache: textures are made at the fitted size, rebuilt only when the
 * revision changes, and dropped with their layers. Runs under SDL_VIDEODRIVER=dummy. */
#include <SDL.h>

#include "layer_thumbnails.h"
#include "minitest.h"

namespace
{

struct Renderer {
    SDL_Window *window;
    SDL_Renderer *renderer;
    Renderer()
        : window(initWindow()), renderer(SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE))
    {
    }
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    Renderer &operator=(Renderer &&) = delete;
    static SDL_Window *initWindow()
    {
        SDL_Init(SDL_INIT_VIDEO);
        return SDL_CreateWindow("test", 0, 0, 100, 100, 0);
    }
    ~Renderer()
    {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

} // namespace

TEST(thumbnail_has_the_fitted_size)
{
    Renderer r;
    LayerThumbnails thumbs(r.renderer);
    Sprite sprite(40, 32);
    LayerThumbnails::Thumb t = thumbs.get(*sprite.layer(0), 0, 36, 24, 1);
    ASSERT_TRUE(t.texture != nullptr);
    ASSERT_EQ_INT(30, t.width);
    ASSERT_EQ_INT(24, t.height);
}

TEST(thumbnail_is_kept_until_the_revision_changes)
{
    Renderer r;
    LayerThumbnails thumbs(r.renderer);
    Sprite sprite(8, 8);
    SDL_Texture *first = thumbs.get(*sprite.layer(0), 0, 36, 24, 1).texture;
    ASSERT_TRUE(first == thumbs.get(*sprite.layer(0), 0, 36, 24, 1).texture);
    /* A new revision refreshes the pixels in the same texture; a different size replaces it. */
    ASSERT_TRUE(first == thumbs.get(*sprite.layer(0), 0, 36, 24, 2).texture);
    LayerThumbnails::Thumb bigger = thumbs.get(*sprite.layer(0), 0, 72, 48, 2);
    ASSERT_EQ_INT(48, bigger.width);
    ASSERT_EQ_INT(1, static_cast<int>(thumbs.size()));
}

TEST(each_layer_has_its_own_thumbnail_and_prune_drops_the_gone_ones)
{
    Renderer r;
    LayerThumbnails thumbs(r.renderer);
    Sprite sprite(8, 8);
    sprite.addLayer("two");
    SDL_Texture *a = thumbs.get(*sprite.layer(0), 0, 36, 24, 1).texture;
    SDL_Texture *b = thumbs.get(*sprite.layer(1), 0, 36, 24, 1).texture;
    ASSERT_TRUE(a != nullptr && b != nullptr && a != b);
    ASSERT_EQ_INT(2, static_cast<int>(thumbs.size()));
    Sprite other(8, 8);
    thumbs.prune(other); /* neither layer belongs to this sprite */
    ASSERT_EQ_INT(0, static_cast<int>(thumbs.size()));
}

TEST(a_missing_frame_gives_no_thumbnail)
{
    Renderer r;
    LayerThumbnails thumbs(r.renderer);
    Sprite sprite(8, 8);
    ASSERT_TRUE(thumbs.get(*sprite.layer(0), 5, 36, 24, 1).texture == nullptr);
    ASSERT_EQ_INT(0, static_cast<int>(thumbs.size()));
}

int main()
{
    RUN(thumbnail_has_the_fitted_size);
    RUN(thumbnail_is_kept_until_the_revision_changes);
    RUN(each_layer_has_its_own_thumbnail_and_prune_drops_the_gone_ones);
    RUN(a_missing_frame_gives_no_thumbnail);
    return TEST_SUMMARY();
}
