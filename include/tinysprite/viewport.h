#ifndef TINYSPRITE_VIEWPORT_H
#define TINYSPRITE_VIEWPORT_H

#define VIEWPORT_ZOOM_MIN 0.0625f /* 1/16: a 4096 px sprite still fits in 256 px */
#define VIEWPORT_ZOOM_MAX 64.0f

/* viewport_screen_to_cel_* saturates at +-VIEWPORT_CEL_LIMIT (2^24, exact in a float) so that
 * extreme positions, such as the off-window mouse position, never overflow an int. */
#define VIEWPORT_CEL_LIMIT 16777216

/* Maps between screen space and cel space. Cel (0, 0)'s top-left corner sits at (pan_x, pan_y) in
 * screen space and each cel pixel covers `zoom` screen pixels. Pure math: no SDL, no sprite. */
typedef struct Viewport {
    float zoom;
    float pan_x, pan_y;
} Viewport;

/* Zoom 1, no pan. */
Viewport viewport_default(void);

/* Cel pixel under a screen position; floors, so positions left of / above the sprite give
 * negative coordinates. Saturates at +-VIEWPORT_CEL_LIMIT; NaN maps to 0. */
int viewport_screen_to_cel_x(const Viewport *view, float screen_x);
int viewport_screen_to_cel_y(const Viewport *view, float screen_y);

/* Screen position of a cel pixel's top-left corner. */
float viewport_cel_to_screen_x(const Viewport *view, int cel_x);
float viewport_cel_to_screen_y(const Viewport *view, int cel_y);

/* Sets the zoom (clamped to VIEWPORT_ZOOM_MIN..MAX) and adjusts the pan so the cel point under
 * the screen position (anchor_x, anchor_y) stays where it is. */
void viewport_zoom_at(Viewport *view, float zoom, float anchor_x, float anchor_y);

/* Next zoom level one step in/out (direction > 0 doubles, < 0 halves), clamped. */
float viewport_next_zoom(float zoom, int direction);

/* Largest zoom that fits the sprite into an area_w x area_h screen area, with the sprite centered
 * in that area: a whole number when the sprite fits at 1x or more, otherwise a power of two below
 * 1 (never below VIEWPORT_ZOOM_MIN). Does nothing if the sprite has no
 * pixels. */
void viewport_fit(Viewport *view, int sprite_w, int sprite_h, float area_w, float area_h);

#endif
