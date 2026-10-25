#ifndef TINYSPRITE_CORE_MOVE_ELEMENT_HPP
#define TINYSPRITE_CORE_MOVE_ELEMENT_HPP

#include <algorithm>
#include <vector>

/* Moves the element at `from` so that it ends up at `to`, shifting the ones between. False, with
 * nothing changed, if either index is out of range. */
template <class T> bool move_element(std::vector<T> &items, int from, int to)
{
    const int count = static_cast<int>(items.size());
    if (from < 0 || from >= count || to < 0 || to >= count) {
        return false;
    }
    auto first = items.begin();
    if (from < to) {
        std::rotate(first + from, first + from + 1, first + to + 1);
    } else {
        std::rotate(first + to, first + from, first + from + 1);
    }
    return true;
}

#endif
