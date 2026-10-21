#ifndef TINYSPRITE_BUCKET_H
#define TINYSPRITE_BUCKET_H

#include "tinysprite/history.h"
#include "tinysprite/symmetry.h"

/* Replaces the 4-connected region of exactly-equal pixels around (x, y) with `color` as one undo
 * step. Filling with the color the region already has records nothing. Returns 0 on success, -1 on
 * invalid arguments, (x, y) out of bounds, an active stroke, or allocation failure (the cel is
 * then left unchanged). */
int bucket_fill(History *history, Cel *cel, int x, int y, Rgba color);

/* bucket_fill that also fills the region under each reflection of (x, y), all in one undo step. */
int bucket_fill_ex(History *history, Cel *cel, int x, int y, Rgba color, Symmetry symmetry);

#endif
