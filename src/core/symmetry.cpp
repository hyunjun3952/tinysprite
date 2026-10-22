#include "tinysprite/symmetry.hpp"

#include <algorithm>

int symmetry_count(Symmetry symmetry)
{
    return (symmetry.mirror_x ? 2 : 1) * (symmetry.mirror_y ? 2 : 1);
}

ToolPoint symmetry_copy(Symmetry symmetry, int width, int height, ToolPoint point, int index)
{
    /* The bits of `index` select which enabled axes this copy flips. */
    int bit = 0;
    bool flipX = symmetry.mirror_x && ((index >> bit++) & 1) != 0;
    bool flipY = symmetry.mirror_y && ((index >> bit) & 1) != 0;
    return ToolPoint{flipX ? width - 1 - point.x : point.x, flipY ? height - 1 - point.y : point.y};
}

std::vector<ToolPoint> symmetry_points(Symmetry symmetry, int width, int height, ToolPoint point)
{
    std::vector<ToolPoint> out;
    for (int i = 0; i < symmetry_count(symmetry); i++) {
        ToolPoint copy = symmetry_copy(symmetry, width, height, point, i);
        bool seen = std::any_of(out.begin(), out.end(),
                                [&](ToolPoint p) { return p.x == copy.x && p.y == copy.y; });
        if (!seen) {
            out.push_back(copy);
        }
    }
    return out;
}
