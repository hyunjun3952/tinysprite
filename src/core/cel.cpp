#include "tinysprite/cel.h"

#include <stdexcept>

Cel::Cel(int width, int height) : width_(width), height_(height)
{
    if (width < 1 || width > CEL_MAX_DIMENSION || height < 1 || height > CEL_MAX_DIMENSION) {
        throw std::invalid_argument("cel dimension out of range");
    }
    pixels_.assign(static_cast<size_t>(width) * static_cast<size_t>(height), Rgba{});
}

Rgba Cel::get(int x, int y) const
{
    if (!inBounds(x, y)) {
        return Rgba{};
    }
    return pixels_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)];
}

bool Cel::set(int x, int y, Rgba color)
{
    if (!inBounds(x, y)) {
        return false;
    }
    pixels_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)] = color;
    return true;
}
