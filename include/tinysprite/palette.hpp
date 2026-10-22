#ifndef TINYSPRITE_PALETTE_HPP
#define TINYSPRITE_PALETTE_HPP

#include <cstdint>
#include <vector>

inline constexpr int PALETTE_MAX_COLORS = 256;

struct Rgba {
    uint8_t r = 0, g = 0, b = 0, a = 0; /* default: transparent black */

    friend bool operator==(Rgba a, Rgba b)
    {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    }
    friend bool operator!=(Rgba a, Rgba b)
    {
        return !(a == b);
    }
};

class Palette
{
  public:
    int count() const
    {
        return static_cast<int>(colors_.size());
    }

    /* Appends a color and returns its index, or -1 if the palette is full. */
    int add(Rgba color);

    /* Returns transparent black for an out-of-range index. */
    Rgba get(int index) const;

    /* Returns false if index is out of range. */
    bool set(int index, Rgba color);

    /* Removes the color at `index`; the colors after it shift down. Returns false if index is out
     * of range. */
    bool remove(int index);

    /* Returns the index of the first color equal to `color` (all four channels), or -1. */
    int find(Rgba color) const;

    /* Same colors in the same order. */
    friend bool operator==(const Palette &a, const Palette &b)
    {
        return a.colors_ == b.colors_;
    }
    friend bool operator!=(const Palette &a, const Palette &b)
    {
        return !(a == b);
    }

  private:
    std::vector<Rgba> colors_;
};

inline constexpr int RECENT_COLORS_MAX = 16;

/* The colors drawn with most recently, newest first, without repeats. */
class RecentColors
{
  public:
    /* Puts `color` first. A color already in the list moves up instead of appearing twice, and the
     * oldest one drops off when the list is full. */
    void push(Rgba color);

    int size() const
    {
        return static_cast<int>(colors_.size());
    }
    Rgba operator[](int index) const
    {
        return colors_[static_cast<size_t>(index)];
    }

  private:
    std::vector<Rgba> colors_;
};

#endif
