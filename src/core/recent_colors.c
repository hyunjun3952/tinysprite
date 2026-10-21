#include "tinysprite/recent_colors.h"

void recent_colors_push(RecentColors *recent, Rgba color)
{
    if (!recent) {
        return;
    }
    /* The slot that ends up free at the front's expense: the color's current position, else the
     * first unused one, else (full list) the oldest entry, which drops off. */
    int slot = recent->count < RECENT_COLORS_MAX ? recent->count : RECENT_COLORS_MAX - 1;
    for (int i = 0; i < recent->count; i++) {
        if (rgba_equal(recent->colors[i], color)) {
            slot = i;
            break;
        }
    }
    if (slot == recent->count) {
        recent->count++;
    }
    for (int i = slot; i > 0; i--) {
        recent->colors[i] = recent->colors[i - 1];
    }
    recent->colors[0] = color;
}
