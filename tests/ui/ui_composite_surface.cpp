/* The texture the canvas shows and the dirty rectangle that decides what is re-composited into it.
 * Runs under SDL_VIDEODRIVER=dummy. */
#include "composite_surface.hpp"
#include "minitest.hpp"
#include "tinysprite/sprite.hpp"
#include "ui_editor_support.hpp"

TEST(a_new_surface_has_a_texture_and_needs_a_full_composite)
{
    auto surface = CompositeSurface::create(renderer, 8, 6);
    ASSERT_TRUE(surface != nullptr);
    ASSERT_TRUE(surface->texture() != nullptr);
    ASSERT_TRUE(surface->dirty());
    Rect r = surface->dirtyRect();
    ASSERT_EQ_INT(0, r.x);
    ASSERT_EQ_INT(0, r.y);
    ASSERT_EQ_INT(8, r.w);
    ASSERT_EQ_INT(6, r.h);
}

TEST(an_invalid_size_gives_no_surface)
{
    ASSERT_TRUE(CompositeSurface::create(renderer, 0, 6) == nullptr);
}

TEST(updating_composites_the_dirty_area_and_clears_the_flag)
{
    Sprite sprite(8, 6);
    sprite.layer(0)->cel(0)->set(1, 1, Rgba{9, 8, 7, 255});
    auto surface = CompositeSurface::create(renderer, 8, 6);
    surface->update(sprite, 0);
    ASSERT_TRUE(!surface->dirty());
    ASSERT_EQ_INT(9, surface->pixelAt(1, 1).r);
}

TEST(only_the_dirty_rectangle_is_recomposited)
{
    Sprite sprite(8, 6);
    auto surface = CompositeSurface::create(renderer, 8, 6);
    surface->update(sprite, 0);
    sprite.layer(0)->cel(0)->set(1, 1, Rgba{1, 2, 3, 255});
    sprite.layer(0)->cel(0)->set(6, 4, Rgba{4, 5, 6, 255});
    surface->markDirty(Rect{0, 0, 3, 3});
    surface->update(sprite, 0);
    ASSERT_EQ_INT(255, surface->pixelAt(1, 1).a);
    ASSERT_EQ_INT(0, surface->pixelAt(6, 4).a); /* outside what was marked */
}

TEST(marking_unions_the_areas_until_the_next_update)
{
    auto surface = CompositeSurface::create(renderer, 8, 6);
    Sprite sprite(8, 6);
    surface->update(sprite, 0);
    surface->markDirty(Rect{1, 1, 2, 2});
    surface->markDirty(Rect{5, 3, 2, 2});
    Rect r = surface->dirtyRect();
    ASSERT_EQ_INT(1, r.x);
    ASSERT_EQ_INT(1, r.y);
    ASSERT_EQ_INT(6, r.w);
    ASSERT_EQ_INT(4, r.h);
    surface->markAllDirty();
    ASSERT_EQ_INT(8, surface->dirtyRect().w);
}

TEST(an_area_outside_the_surface_is_clipped_and_leaves_it_clean)
{
    Sprite sprite(8, 6);
    auto surface = CompositeSurface::create(renderer, 8, 6);
    surface->update(sprite, 0);
    surface->markDirty(Rect{20, 20, 3, 3});
    surface->update(sprite, 0);
    ASSERT_TRUE(!surface->dirty());
}

int main()
{
    if (!ui_init()) {
        return 1;
    }
    RUN(a_new_surface_has_a_texture_and_needs_a_full_composite);
    RUN(an_invalid_size_gives_no_surface);
    RUN(updating_composites_the_dirty_area_and_clears_the_flag);
    RUN(only_the_dirty_rectangle_is_recomposited);
    RUN(marking_unions_the_areas_until_the_next_update);
    RUN(an_area_outside_the_surface_is_clipped_and_leaves_it_clean);
    ui_quit();
    return TEST_SUMMARY();
}
