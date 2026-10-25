#include "ruler.hpp"

#include <cmath>

#include "tinysprite/viewport.hpp"

int ruler_step(float zoom, float gap)
{
    constexpr int MULTIPLES[] = {1, 2, 5};
    constexpr int LARGEST = VIEWPORT_CEL_LIMIT;
    for (int decade = 1; decade <= LARGEST / 10; decade *= 10) {
        for (const int multiple : MULTIPLES) {
            const int step = decade * multiple;
            if (static_cast<float>(step) * zoom >= gap) {
                return step;
            }
        }
    }
    return LARGEST;
}

int guide_snap(float pan, float zoom, float at)
{
    const auto limit = static_cast<float>(VIEWPORT_CEL_LIMIT);
    const float edge = std::floor((at - pan) / zoom + 0.5f);
    if (!(edge > -limit)) { /* also catches NaN */
        return -VIEWPORT_CEL_LIMIT;
    }
    return edge < limit ? static_cast<int>(edge) : VIEWPORT_CEL_LIMIT;
}

int guide_pick(const std::vector<int> &lines, float pan, float zoom, float at)
{
    int best = -1;
    float bestDistance = GUIDE_REACH;
    for (int i = 0; i < static_cast<int>(lines.size()); i++) {
        const float screen = pan + static_cast<float>(lines[static_cast<size_t>(i)]) * zoom;
        const float distance = std::fabs(screen - at);
        if (distance <= bestDistance) {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}
