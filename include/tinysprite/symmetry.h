#ifndef TINYSPRITE_SYMMETRY_H
#define TINYSPRITE_SYMMETRY_H

#include <stdbool.h>

typedef struct ToolPoint {
    int x, y;
} ToolPoint;

/* Mirror axes through the center of a cel: a point (x, y) is reflected to (width - 1 - x, y) with
 * mirror_x and to (x, height - 1 - y) with mirror_y. All fields default to 0 (off). */
typedef struct Symmetry {
    bool mirror_x; /* reflect left <-> right */
    bool mirror_y; /* reflect top <-> bottom */
} Symmetry;

#define SYMMETRY_NONE ((Symmetry){false, false})

/* How many copies of a drawing the enabled axes produce: 1, 2 or 4. */
int symmetry_count(Symmetry symmetry);

/* Copy `index` (0 .. symmetry_count - 1) of `point` in a width x height cel; copy 0 is the point
 * itself. Shapes reflect both of their end points with the same index, which reflects the whole
 * shape. */
ToolPoint symmetry_copy(Symmetry symmetry, int width, int height, ToolPoint point, int index);

/* The distinct copies of one point, the point itself first, written to `out`. A point on an axis
 * is its own reflection, so it is listed once. Returns how many were written. */
int symmetry_points(Symmetry symmetry, int width, int height, ToolPoint point, ToolPoint out[4]);

#endif
