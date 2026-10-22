#include <cmath>

#include "minitest.hpp"
#include "tinysprite/viewport.hpp"

TEST(default_view_maps_origin_to_screen_origin)
{
    auto v = Viewport{};
    ASSERT_EQ_INT(0, v.screenToCelX(0.0f));
    ASSERT_EQ_INT(0, v.screenToCelY(0.0f));
    ASSERT_EQ_INT(5, v.screenToCelX(5.9f));
}

TEST(zoom_scales_cel_coordinates)
{
    auto v = Viewport{};
    v.zoom = 4.0f;
    ASSERT_EQ_INT(0, v.screenToCelX(3.9f));
    ASSERT_EQ_INT(1, v.screenToCelX(4.0f));
    ASSERT_EQ_INT(2, v.screenToCelY(9.0f));
}

TEST(pan_offsets_cel_coordinates)
{
    auto v = Viewport{};
    v.zoom = 2.0f;
    v.pan_x = 10.0f;
    v.pan_y = -4.0f;
    ASSERT_EQ_INT(0, v.screenToCelX(10.0f));
    ASSERT_EQ_INT(3, v.screenToCelX(16.0f));
    ASSERT_EQ_INT(0, v.screenToCelY(-4.0f));
}

TEST(negative_screen_positions_floor_to_negative_cels)
{
    auto v = Viewport{};
    v.zoom = 4.0f;
    ASSERT_EQ_INT(-1, v.screenToCelX(-0.5f));
    ASSERT_EQ_INT(-2, v.screenToCelX(-4.5f));
}

TEST(cel_to_screen_returns_top_left_corner)
{
    auto v = Viewport{};
    v.zoom = 8.0f;
    v.pan_x = 3.0f;
    v.pan_y = 5.0f;
    ASSERT_TRUE(v.celToScreenX(2) == 19.0f);
    ASSERT_TRUE(v.celToScreenY(0) == 5.0f);
}

TEST(round_trip_hits_same_cel)
{
    auto v = Viewport{};
    v.zoom = 6.0f;
    v.pan_x = 17.0f;
    v.pan_y = 9.0f;
    for (int c = -3; c < 20; c++) {
        float sx = v.celToScreenX(c) + 0.5f;
        ASSERT_EQ_INT(c, v.screenToCelX(sx));
    }
}

TEST(zoom_at_keeps_anchor_point_fixed)
{
    auto v = Viewport{};
    v.zoom = 4.0f;
    v.pan_x = 20.0f;
    v.pan_y = 30.0f;
    float ax = 100.0f, ay = 80.0f;
    float before_x = (ax - v.pan_x) / v.zoom;
    float before_y = (ay - v.pan_y) / v.zoom;
    v.zoomAt(8.0f, ax, ay);
    ASSERT_TRUE(v.zoom == 8.0f);
    ASSERT_TRUE((ax - v.pan_x) / v.zoom == before_x);
    ASSERT_TRUE((ay - v.pan_y) / v.zoom == before_y);
}

TEST(zoom_is_clamped)
{
    auto v = Viewport{};
    v.zoomAt(1000.0f, 0.0f, 0.0f);
    ASSERT_TRUE(v.zoom == VIEWPORT_ZOOM_MAX);
    v.zoomAt(0.0f, 0.0f, 0.0f);
    ASSERT_TRUE(v.zoom == VIEWPORT_ZOOM_MIN);
}

TEST(step_zoom_doubles_and_halves)
{
    ASSERT_TRUE(Viewport::nextZoom(4.0f, 1) == 8.0f);
    ASSERT_TRUE(Viewport::nextZoom(4.0f, -1) == 2.0f);
    ASSERT_TRUE(Viewport::nextZoom(VIEWPORT_ZOOM_MAX, 1) == VIEWPORT_ZOOM_MAX);
    ASSERT_TRUE(Viewport::nextZoom(VIEWPORT_ZOOM_MIN, -1) == VIEWPORT_ZOOM_MIN);
}

TEST(fit_centers_sprite_in_area)
{
    auto v = Viewport{};
    v.fit(16, 16, 200.0f, 100.0f);
    /* Largest integer zoom that fits the shorter side: 100 / 16 -> 6. */
    ASSERT_TRUE(v.zoom == 6.0f);
    ASSERT_TRUE(v.pan_x == (200.0f - 16 * 6.0f) / 2.0f);
    ASSERT_TRUE(v.pan_y == (100.0f - 16 * 6.0f) / 2.0f);
}

TEST(fit_never_goes_below_minimum_zoom)
{
    auto v = Viewport{};
    v.fit(4096, 4096, 10.0f, 10.0f);
    ASSERT_TRUE(v.zoom == VIEWPORT_ZOOM_MIN);
}

TEST(extreme_screen_positions_are_clamped)
{
    auto v = Viewport{};
    v.zoom = 4.0f;
    ASSERT_EQ_INT(-VIEWPORT_CEL_LIMIT, v.screenToCelX(-3.4e38f));
    ASSERT_EQ_INT(VIEWPORT_CEL_LIMIT, v.screenToCelY(3.4e38f));
    ASSERT_EQ_INT(-VIEWPORT_CEL_LIMIT, v.screenToCelX(-INFINITY));
    ASSERT_EQ_INT(VIEWPORT_CEL_LIMIT, v.screenToCelX(INFINITY));
    ASSERT_EQ_INT(0, v.screenToCelX(NAN));
}

TEST(huge_pan_is_clamped)
{
    auto v = Viewport{};
    v.pan_x = 1e30f;
    v.pan_y = -1e30f;
    ASSERT_EQ_INT(-VIEWPORT_CEL_LIMIT, v.screenToCelX(0.0f));
    ASSERT_EQ_INT(VIEWPORT_CEL_LIMIT, v.screenToCelY(0.0f));
}

TEST(nan_zoom_falls_back_to_minimum)
{
    auto v = Viewport{};
    v.zoomAt(NAN, 10.0f, 10.0f);
    ASSERT_TRUE(v.zoom == VIEWPORT_ZOOM_MIN);
    ASSERT_TRUE(Viewport::nextZoom(NAN, 1) == VIEWPORT_ZOOM_MIN);
}

TEST(fit_ignores_empty_sprite_or_area)
{
    auto v = Viewport{};
    v.zoom = 3.0f;
    v.pan_x = 7.0f;
    v.fit(0, 16, 100.0f, 100.0f);
    v.fit(16, -1, 100.0f, 100.0f);
    ASSERT_TRUE(v.zoom == 3.0f && v.pan_x == 7.0f);
    v.fit(16, 16, 0.0f, -5.0f);
    ASSERT_TRUE(v.zoom == VIEWPORT_ZOOM_MIN);
}

TEST(zoom_can_go_below_one)
{
    ASSERT_TRUE(VIEWPORT_ZOOM_MIN < 1.0f);
    ASSERT_TRUE(Viewport::nextZoom(1.0f, -1) == 0.5f);
    ASSERT_TRUE(Viewport::nextZoom(0.5f, -1) == 0.25f);
    ASSERT_TRUE(Viewport::nextZoom(0.25f, 1) == 0.5f);
}

TEST(fractional_zoom_maps_many_cels_per_screen_pixel)
{
    auto v = Viewport{};
    v.zoom = 0.25f;
    ASSERT_EQ_INT(15, v.screenToCelX(3.9f));
    ASSERT_EQ_INT(4, v.screenToCelX(1.0f));
    ASSERT_TRUE(v.celToScreenX(8) == 2.0f);
}

TEST(fit_shrinks_to_a_power_of_two_for_large_sprites)
{
    auto v = Viewport{};
    v.fit(4096, 4096, 512.0f, 600.0f);
    ASSERT_TRUE(v.zoom == 0.125f);
    ASSERT_TRUE(v.pan_x == 0.0f);
    ASSERT_TRUE(v.pan_y == (600.0f - 512.0f) / 2.0f);
    v.fit(100, 100, 30.0f, 30.0f);
    ASSERT_TRUE(v.zoom == 0.25f);
}

int main()
{
    RUN(default_view_maps_origin_to_screen_origin);
    RUN(zoom_scales_cel_coordinates);
    RUN(pan_offsets_cel_coordinates);
    RUN(negative_screen_positions_floor_to_negative_cels);
    RUN(cel_to_screen_returns_top_left_corner);
    RUN(round_trip_hits_same_cel);
    RUN(zoom_at_keeps_anchor_point_fixed);
    RUN(zoom_is_clamped);
    RUN(step_zoom_doubles_and_halves);
    RUN(fit_centers_sprite_in_area);
    RUN(fit_never_goes_below_minimum_zoom);
    RUN(extreme_screen_positions_are_clamped);
    RUN(huge_pan_is_clamped);
    RUN(nan_zoom_falls_back_to_minimum);
    RUN(fit_ignores_empty_sprite_or_area);
    RUN(zoom_can_go_below_one);
    RUN(fractional_zoom_maps_many_cels_per_screen_pixel);
    RUN(fit_shrinks_to_a_power_of_two_for_large_sprites);
    return TEST_SUMMARY();
}
