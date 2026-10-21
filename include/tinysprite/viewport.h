#ifndef TINYSPRITE_VIEWPORT_H
#define TINYSPRITE_VIEWPORT_H

#define VIEWPORT_ZOOM_MIN 1.0f
#define VIEWPORT_ZOOM_MAX 64.0f

/* Maps between screen space and cel space. Cel (0, 0)'s top-left corner sits at (pan_x, pan_y) in
 * screen space and each cel pixel covers `zoom` screen pixels. Pure math: no SDL, no sprite. */
typedef struct Viewport {
    float zoom;
    float pan_x, pan_y;
} Viewport;

/* Zoom 1, no pan. */
Viewport viewport_default(void);

/* Cel pixel under a screen position; floors, so positions left of / above the sprite give
 * negative coordinates. */
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

/* Largest whole-number zoom that fits the sprite into an area_w x area_h screen area (at least
 * VIEWPORT_ZOOM_MIN), with the sprite centered in that area. */
void viewport_fit(Viewport *view, int sprite_w, int sprite_h, float area_w, float area_h);

#endif
