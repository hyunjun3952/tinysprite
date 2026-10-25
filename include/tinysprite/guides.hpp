#ifndef TINYSPRITE_GUIDES_HPP
#define TINYSPRITE_GUIDES_HPP

#include <vector>

/* The guides of a document: vertical lines by their x and horizontal lines by their y, in cel
 * edges (a guide at 8 lies between cel 7 and cel 8). They live on the Sprite and are saved in the
 * .pxa, but they are not an undo step. */
struct Guides {
    std::vector<int> vertical;
    std::vector<int> horizontal;

    bool empty() const
    {
        return vertical.empty() && horizontal.empty();
    }
    void clear()
    {
        vertical.clear();
        horizontal.clear();
    }
};

#endif
