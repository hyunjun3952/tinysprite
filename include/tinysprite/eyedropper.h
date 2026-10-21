#ifndef TINYSPRITE_EYEDROPPER_H
#define TINYSPRITE_EYEDROPPER_H

#include "tinysprite/sprite.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the color at (x, y) of a single cel. Returns 0 on success, -1 (leaving `out` untouched) if
 * an argument is NULL or (x, y) is out of bounds. */
int eyedropper_pick_cel(const Cel *cel, int x, int y, Rgba *out);

/* Reads the color at (x, y) of the flattened frame (visible layers, as displayed). Returns 0 on
 * success, -1 (leaving `out` untouched) if an argument is NULL, the frame is out of range, or
 * (x, y) is out of bounds. */
int eyedropper_pick_sprite(Sprite *sprite, int frame, int x, int y, Rgba *out);

#ifdef __cplusplus
}
#endif

#endif
