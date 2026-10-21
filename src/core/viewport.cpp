#include "tinysprite/viewport.h"

#include <cmath>

static float clamp_zoom(float zoom)
{
    /* Written so that NaN falls through to the minimum. */
    if (!(zoom >= VIEWPORT_ZOOM_MIN)) {
        return VIEWPORT_ZOOM_MIN;
    }
    if (zoom > VIEWPORT_ZOOM_MAX) {
        return VIEWPORT_ZOOM_MAX;
    }
    return zoom;
}

/* Floors to a cel coordinate, saturating at +-VIEWPORT_CEL_LIMIT; NaN becomes 0. */
static int to_cel(float screen, float pan, float zoom)
{
    float cel = std::floor((screen - pan) / zoom);
    if (std::isnan(cel)) {
        return 0;
    }
    if (cel <= -static_cast<float>(VIEWPORT_CEL_LIMIT)) {
        return -VIEWPORT_CEL_LIMIT;
    }
    if (cel >= static_cast<float>(VIEWPORT_CEL_LIMIT)) {
        return VIEWPORT_CEL_LIMIT;
    }
    return static_cast<int>(cel);
}

int Viewport::screenToCelX(float screen_x) const
{
    return to_cel(screen_x, pan_x, zoom);
}

int Viewport::screenToCelY(float screen_y) const
{
    return to_cel(screen_y, pan_y, zoom);
}

float Viewport::celToScreenX(int cel_x) const
{
    return pan_x + static_cast<float>(cel_x) * zoom;
}

float Viewport::celToScreenY(int cel_y) const
{
    return pan_y + static_cast<float>(cel_y) * zoom;
}

void Viewport::zoomAt(float newZoom, float anchor_x, float anchor_y)
{
    float next = clamp_zoom(newZoom);
    float cel_x = (anchor_x - pan_x) / zoom;
    float cel_y = (anchor_y - pan_y) / zoom;
    zoom = next;
    pan_x = anchor_x - cel_x * next;
    pan_y = anchor_y - cel_y * next;
}

float Viewport::nextZoom(float zoom, int direction)
{
    if (direction > 0) {
        return clamp_zoom(zoom * 2.0f);
    }
    if (direction < 0) {
        return clamp_zoom(zoom / 2.0f);
    }
    return clamp_zoom(zoom);
}

void Viewport::fit(int sprite_w, int sprite_h, float area_w, float area_h)
{
    if (sprite_w < 1 || sprite_h < 1) {
        return;
    }
    float fit_x = area_w / static_cast<float>(sprite_w);
    float fit_y = area_h / static_cast<float>(sprite_h);
    float fit = fit_x < fit_y ? fit_x : fit_y;
    float next;
    if (fit >= 1.0f) {
        next = clamp_zoom(std::floor(fit));
    } else {
        /* Halve from 1x so downscaled views stay on power-of-two steps. */
        next = 1.0f;
        while (next > fit && next > VIEWPORT_ZOOM_MIN) {
            next /= 2.0f;
        }
        next = clamp_zoom(next);
    }
    zoom = next;
    pan_x = (area_w - static_cast<float>(sprite_w) * zoom) / 2.0f;
    pan_y = (area_h - static_cast<float>(sprite_h) * zoom) / 2.0f;
}
