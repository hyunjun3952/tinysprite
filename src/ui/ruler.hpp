#ifndef TINYSPRITE_UI_RULER_HPP
#define TINYSPRITE_UI_RULER_HPP

#include <vector>

/* Rulers along the top and left edge of the canvas and the guides dragged out of them: the maths
 * only (tick spacing, picking and snapping a guide); the canvas draws them. Positions are cel
 * edges, so a guide at 8 lies between cel 7 and cel 8. */

inline constexpr float RULER_THICKNESS = 20.0f; /* at UI scale 1 */
inline constexpr float RULER_LABEL_GAP = 56.0f; /* least screen distance between two labels */
inline constexpr float GUIDE_REACH = 4.0f;      /* how close the mouse must be to grab a guide */

/* The guides of a document: vertical lines by their x and horizontal lines by their y. They belong
 * to the view, not to the sprite: they are not saved and not undoable. */
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

/* Cels between two labelled ticks at `zoom`: the smallest of 1, 2, 5, 10, 20, 50, ... that keeps
 * the labels RULER_LABEL_GAP apart on screen. */
int ruler_step(float zoom);

/* The cel edge nearest to the screen position `at` on an axis whose cel 0 starts at `pan`. */
int guide_snap(float pan, float zoom, float at);

/* Index of the line in `lines` within GUIDE_REACH of the screen position `at`, or -1; the nearest
 * one when several are in reach. */
int guide_pick(const std::vector<int> &lines, float pan, float zoom, float at);

#endif
