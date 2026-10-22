#include "window_size.hpp"

#include <algorithm>
#include <cmath>

namespace
{

constexpr int MIN_WIDTH = 640;
constexpr int MIN_HEIGHT = 420;

} // namespace

WindowSize window_fit(WindowSize wanted, WindowSize usable)
{
    if (usable.width > 0) {
        wanted.width = std::min(wanted.width, usable.width - WINDOW_FRAME_MARGIN);
    }
    if (usable.height > 0) {
        wanted.height = std::min(wanted.height, usable.height - WINDOW_FRAME_MARGIN);
    }
    return wanted;
}

WindowSize window_minimum(float scale)
{
    return WindowSize{static_cast<int>(std::lround(MIN_WIDTH * scale)),
                      static_cast<int>(std::lround(MIN_HEIGHT * scale))};
}
