#ifndef TINYSPRITE_VIEWPORT_HPP
#define TINYSPRITE_VIEWPORT_HPP

inline constexpr float VIEWPORT_ZOOM_MIN =
    0.0625f; /* 1/16: a 4096 px sprite still fits in 256 px */
inline constexpr float VIEWPORT_ZOOM_MAX = 64.0f;

/* screenToCel* saturates at +-VIEWPORT_CEL_LIMIT (2^24, exact in a float) so that extreme
 * positions, such as the off-window mouse position, never overflow an int. */
inline constexpr int VIEWPORT_CEL_LIMIT = 16777216;

/* Maps between screen space and cel space. Cel (0, 0)'s top-left corner sits at (pan_x, pan_y) in
 * screen space and each cel pixel covers `zoom` screen pixels. Pure math: no SDL, no sprite.
 * A default viewport has zoom 1 and no pan. */
struct Viewport {
    float zoom = 1.0f;
    float pan_x = 0.0f, pan_y = 0.0f;

    /* Cel pixel under a screen position; floors, so positions left of / above the sprite give
     * negative coordinates. Saturates at +-VIEWPORT_CEL_LIMIT; NaN maps to 0. */
    int screenToCelX(float screen_x) const;
    int screenToCelY(float screen_y) const;

    /* Screen position of a cel pixel's top-left corner. */
    float celToScreenX(int cel_x) const;
    float celToScreenY(int cel_y) const;

    /* Sets the zoom (clamped to VIEWPORT_ZOOM_MIN..MAX) and adjusts the pan so the cel point under
     * the screen position (anchor_x, anchor_y) stays where it is. */
    void zoomAt(float zoom, float anchor_x, float anchor_y);

    /* Largest zoom that fits the sprite into an area_w x area_h screen area, with the sprite
     * centered in that area: a whole number when the sprite fits at 1x or more, otherwise a power
     * of two below 1 (never below VIEWPORT_ZOOM_MIN). Does nothing if the sprite has no pixels. */
    void fit(int sprite_w, int sprite_h, float area_w, float area_h);

    /* Next zoom level one step in/out (direction > 0 doubles, < 0 halves), clamped. */
    static float nextZoom(float zoom, int direction);
};

#endif
