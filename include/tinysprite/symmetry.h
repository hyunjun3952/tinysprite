#ifndef TINYSPRITE_SYMMETRY_H
#define TINYSPRITE_SYMMETRY_H

/* Mirror axes through the center of a cel: a point (x, y) is reflected to (width - 1 - x, y) with
 * mirror_x and to (x, height - 1 - y) with mirror_y. All fields default to 0 (off). */
typedef struct Symmetry {
    int mirror_x; /* reflect left <-> right */
    int mirror_y; /* reflect top <-> bottom */
} Symmetry;

#endif
