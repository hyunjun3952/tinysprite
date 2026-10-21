#include "tinysprite/viewport.h"

#include <math.h>

static float clamp_zoom(float zoom)
{
    if (zoom < VIEWPORT_ZOOM_MIN) {
        return VIEWPORT_ZOOM_MIN;
    }
    if (zoom > VIEWPORT_ZOOM_MAX) {
        return VIEWPORT_ZOOM_MAX;
    }
    return zoom;
}

Viewport viewport_default(void)
{
    return (Viewport){1.0f, 0.0f, 0.0f};
}

int viewport_screen_to_cel_x(const Viewport *view, float screen_x)
{
    return (int)floorf((screen_x - view->pan_x) / view->zoom);
}

int viewport_screen_to_cel_y(const Viewport *view, float screen_y)
{
    return (int)floorf((screen_y - view->pan_y) / view->zoom);
}

float viewport_cel_to_screen_x(const Viewport *view, int cel_x)
{
    return view->pan_x + (float)cel_x * view->zoom;
}

float viewport_cel_to_screen_y(const Viewport *view, int cel_y)
{
    return view->pan_y + (float)cel_y * view->zoom;
}

void viewport_zoom_at(Viewport *view, float zoom, float anchor_x, float anchor_y)
{
    float next = clamp_zoom(zoom);
    float cel_x = (anchor_x - view->pan_x) / view->zoom;
    float cel_y = (anchor_y - view->pan_y) / view->zoom;
    view->zoom = next;
    view->pan_x = anchor_x - cel_x * next;
    view->pan_y = anchor_y - cel_y * next;
}

float viewport_next_zoom(float zoom, int direction)
{
    if (direction > 0) {
        return clamp_zoom(zoom * 2.0f);
    }
    if (direction < 0) {
        return clamp_zoom(zoom / 2.0f);
    }
    return clamp_zoom(zoom);
}

void viewport_fit(Viewport *view, int sprite_w, int sprite_h, float area_w, float area_h)
{
    float fit_x = area_w / (float)sprite_w;
    float fit_y = area_h / (float)sprite_h;
    float zoom = clamp_zoom(floorf(fit_x < fit_y ? fit_x : fit_y));
    view->zoom = zoom;
    view->pan_x = (area_w - (float)sprite_w * zoom) / 2.0f;
    view->pan_y = (area_h - (float)sprite_h * zoom) / 2.0f;
}
