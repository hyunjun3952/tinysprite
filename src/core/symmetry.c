#include "tinysprite/symmetry.h"

int symmetry_count(Symmetry symmetry)
{
    return (symmetry.mirror_x ? 2 : 1) * (symmetry.mirror_y ? 2 : 1);
}

ToolPoint symmetry_copy(Symmetry symmetry, int width, int height, ToolPoint point, int index)
{
    /* The bits of `index` select which enabled axes this copy flips. */
    int bit = 0;
    bool flip_x = symmetry.mirror_x && ((index >> bit++) & 1);
    bool flip_y = symmetry.mirror_y && ((index >> bit) & 1);
    return (ToolPoint){flip_x ? width - 1 - point.x : point.x,
                       flip_y ? height - 1 - point.y : point.y};
}

int symmetry_points(Symmetry symmetry, int width, int height, ToolPoint point, ToolPoint out[4])
{
    int count = 0;
    for (int i = 0; i < symmetry_count(symmetry); i++) {
        ToolPoint copy = symmetry_copy(symmetry, width, height, point, i);
        bool seen = false;
        for (int j = 0; j < count; j++) {
            seen = seen || (out[j].x == copy.x && out[j].y == copy.y);
        }
        if (!seen) {
            out[count++] = copy;
        }
    }
    return count;
}
