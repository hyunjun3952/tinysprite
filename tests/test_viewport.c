#include <math.h>

#include "minitest.h"
#include "tinysprite/viewport.h"

TEST(default_view_maps_origin_to_screen_origin)
{
    Viewport v = viewport_default();
    ASSERT_EQ_INT(0, viewport_screen_to_cel_x(&v, 0.0f));
    ASSERT_EQ_INT(0, viewport_screen_to_cel_y(&v, 0.0f));
    ASSERT_EQ_INT(5, viewport_screen_to_cel_x(&v, 5.9f));
}

TEST(zoom_scales_cel_coordinates)
{
    Viewport v = viewport_default();
    v.zoom = 4.0f;
    ASSERT_EQ_INT(0, viewport_screen_to_cel_x(&v, 3.9f));
    ASSERT_EQ_INT(1, viewport_screen_to_cel_x(&v, 4.0f));
    ASSERT_EQ_INT(2, viewport_screen_to_cel_y(&v, 9.0f));
}

TEST(pan_offsets_cel_coordinates)
{
    Viewport v = viewport_default();
    v.zoom = 2.0f;
    v.pan_x = 10.0f;
    v.pan_y = -4.0f;
    ASSERT_EQ_INT(0, viewport_screen_to_cel_x(&v, 10.0f));
    ASSERT_EQ_INT(3, viewport_screen_to_cel_x(&v, 16.0f));
    ASSERT_EQ_INT(0, viewport_screen_to_cel_y(&v, -4.0f));
}

TEST(negative_screen_positions_floor_to_negative_cels)
{
    Viewport v = viewport_default();
    v.zoom = 4.0f;
    ASSERT_EQ_INT(-1, viewport_screen_to_cel_x(&v, -0.5f));
    ASSERT_EQ_INT(-2, viewport_screen_to_cel_x(&v, -4.5f));
}

TEST(cel_to_screen_returns_top_left_corner)
{
    Viewport v = viewport_default();
    v.zoom = 8.0f;
    v.pan_x = 3.0f;
    v.pan_y = 5.0f;
    ASSERT_TRUE(viewport_cel_to_screen_x(&v, 2) == 19.0f);
    ASSERT_TRUE(viewport_cel_to_screen_y(&v, 0) == 5.0f);
}

TEST(round_trip_hits_same_cel)
{
    Viewport v = viewport_default();
    v.zoom = 6.0f;
    v.pan_x = 17.0f;
    v.pan_y = 9.0f;
    for (int c = -3; c < 20; c++) {
        float sx = viewport_cel_to_screen_x(&v, c) + 0.5f;
        ASSERT_EQ_INT(c, viewport_screen_to_cel_x(&v, sx));
    }
}

TEST(zoom_at_keeps_anchor_point_fixed)
{
    Viewport v = viewport_default();
    v.zoom = 4.0f;
    v.pan_x = 20.0f;
    v.pan_y = 30.0f;
    float ax = 100.0f, ay = 80.0f;
    float before_x = (ax - v.pan_x) / v.zoom;
    float before_y = (ay - v.pan_y) / v.zoom;
    viewport_zoom_at(&v, 8.0f, ax, ay);
    ASSERT_TRUE(v.zoom == 8.0f);
    ASSERT_TRUE((ax - v.pan_x) / v.zoom == before_x);
    ASSERT_TRUE((ay - v.pan_y) / v.zoom == before_y);
}

TEST(zoom_is_clamped)
{
    Viewport v = viewport_default();
    viewport_zoom_at(&v, 1000.0f, 0.0f, 0.0f);
    ASSERT_TRUE(v.zoom == VIEWPORT_ZOOM_MAX);
    viewport_zoom_at(&v, 0.0f, 0.0f, 0.0f);
    ASSERT_TRUE(v.zoom == VIEWPORT_ZOOM_MIN);
}

TEST(step_zoom_doubles_and_halves)
{
    ASSERT_TRUE(viewport_next_zoom(4.0f, 1) == 8.0f);
    ASSERT_TRUE(viewport_next_zoom(4.0f, -1) == 2.0f);
    ASSERT_TRUE(viewport_next_zoom(VIEWPORT_ZOOM_MAX, 1) == VIEWPORT_ZOOM_MAX);
    ASSERT_TRUE(viewport_next_zoom(VIEWPORT_ZOOM_MIN, -1) == VIEWPORT_ZOOM_MIN);
}

TEST(fit_centers_sprite_in_area)
{
    Viewport v = viewport_default();
    viewport_fit(&v, 16, 16, 200.0f, 100.0f);
    /* Largest integer zoom that fits the shorter side: 100 / 16 -> 6. */
    ASSERT_TRUE(v.zoom == 6.0f);
    ASSERT_TRUE(v.pan_x == (200.0f - 16 * 6.0f) / 2.0f);
    ASSERT_TRUE(v.pan_y == (100.0f - 16 * 6.0f) / 2.0f);
}

TEST(fit_never_goes_below_minimum_zoom)
{
    Viewport v = viewport_default();
    viewport_fit(&v, 4096, 4096, 10.0f, 10.0f);
    ASSERT_TRUE(v.zoom == VIEWPORT_ZOOM_MIN);
}

TEST(extreme_screen_positions_are_clamped)
{
    Viewport v = viewport_default();
    v.zoom = 4.0f;
    ASSERT_EQ_INT(-VIEWPORT_CEL_LIMIT, viewport_screen_to_cel_x(&v, -3.4e38f));
    ASSERT_EQ_INT(VIEWPORT_CEL_LIMIT, viewport_screen_to_cel_y(&v, 3.4e38f));
    ASSERT_EQ_INT(-VIEWPORT_CEL_LIMIT, viewport_screen_to_cel_x(&v, -INFINITY));
    ASSERT_EQ_INT(VIEWPORT_CEL_LIMIT, viewport_screen_to_cel_x(&v, INFINITY));
    ASSERT_EQ_INT(0, viewport_screen_to_cel_x(&v, NAN));
}

TEST(huge_pan_is_clamped)
{
    Viewport v = viewport_default();
    v.pan_x = 1e30f;
    v.pan_y = -1e30f;
    ASSERT_EQ_INT(-VIEWPORT_CEL_LIMIT, viewport_screen_to_cel_x(&v, 0.0f));
    ASSERT_EQ_INT(VIEWPORT_CEL_LIMIT, viewport_screen_to_cel_y(&v, 0.0f));
}

TEST(nan_zoom_falls_back_to_minimum)
{
    Viewport v = viewport_default();
    viewport_zoom_at(&v, NAN, 10.0f, 10.0f);
    ASSERT_TRUE(v.zoom == VIEWPORT_ZOOM_MIN);
    ASSERT_TRUE(viewport_next_zoom(NAN, 1) == VIEWPORT_ZOOM_MIN);
}

TEST(fit_ignores_empty_sprite_or_area)
{
    Viewport v = viewport_default();
    v.zoom = 3.0f;
    v.pan_x = 7.0f;
    viewport_fit(&v, 0, 16, 100.0f, 100.0f);
    viewport_fit(&v, 16, -1, 100.0f, 100.0f);
    ASSERT_TRUE(v.zoom == 3.0f && v.pan_x == 7.0f);
    viewport_fit(&v, 16, 16, 0.0f, -5.0f);
    ASSERT_TRUE(v.zoom == VIEWPORT_ZOOM_MIN);
}

int main(void)
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
    return TEST_SUMMARY();
}
