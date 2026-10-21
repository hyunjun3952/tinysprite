#include "tinysprite/palette.h"

#include <algorithm>

int Palette::add(Rgba color)
{
    if (count() >= PALETTE_MAX_COLORS) {
        return -1;
    }
    colors_.push_back(color);
    return count() - 1;
}

Rgba Palette::get(int index) const
{
    return index >= 0 && index < count() ? colors_[static_cast<size_t>(index)] : Rgba{};
}

bool Palette::set(int index, Rgba color)
{
    if (index < 0 || index >= count()) {
        return false;
    }
    colors_[static_cast<size_t>(index)] = color;
    return true;
}

bool Palette::remove(int index)
{
    if (index < 0 || index >= count()) {
        return false;
    }
    colors_.erase(colors_.begin() + index);
    return true;
}

int Palette::find(Rgba color) const
{
    auto it = std::find(colors_.begin(), colors_.end(), color);
    return it == colors_.end() ? -1 : static_cast<int>(it - colors_.begin());
}

void RecentColors::push(Rgba color)
{
    auto known = std::find(colors_.begin(), colors_.end(), color);
    if (known != colors_.end()) {
        colors_.erase(known);
    } else if (size() == RECENT_COLORS_MAX) {
        colors_.pop_back();
    }
    colors_.insert(colors_.begin(), color);
}
