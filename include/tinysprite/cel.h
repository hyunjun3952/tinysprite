#ifndef TINYSPRITE_CEL_H
#define TINYSPRITE_CEL_H

#include <cstddef>
#include <vector>

#include "tinysprite/palette.h"

inline constexpr int CEL_MAX_DIMENSION = 4096;

/* A width x height RGBA pixel buffer, fully transparent when created. */
class Cel
{
  public:
    /* Throws std::invalid_argument if a dimension is outside 1..CEL_MAX_DIMENSION. */
    Cel(int width, int height);

    int width() const
    {
        return width_;
    }
    int height() const
    {
        return height_;
    }

    bool inBounds(int x, int y) const
    {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }

    /* Position of (x, y) in the pixels() buffer; the caller checks inBounds first. */
    size_t indexOf(int x, int y) const
    {
        return static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x);
    }

    /* Returns transparent black for out-of-bounds coordinates. */
    Rgba get(int x, int y) const;

    /* Returns false if (x, y) is out of bounds. */
    bool set(int x, int y, Rgba color);

    /* Row-major pixel buffer of width * height entries. */
    Rgba *pixels()
    {
        return pixels_.data();
    }
    const Rgba *pixels() const
    {
        return pixels_.data();
    }

  private:
    int width_;
    int height_;
    std::vector<Rgba> pixels_;
};

#endif
