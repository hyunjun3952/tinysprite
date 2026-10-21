#include "tinysprite/recent_colors.h"

static int same_color(Rgba a, Rgba b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

void recent_colors_push(RecentColors *recent, Rgba color)
{
    if (!recent) {
        return;
    }
    /* Find where the color currently is (or the slot that falls off the end), then shift the
     * newer entries down by one to open the front. */
    int slot = recent->count;
    for (int i = 0; i < recent->count; i++) {
        if (same_color(recent->colors[i], color)) {
            slot = i;
            break;
        }
    }
    if (slot == RECENT_COLORS_MAX) {
        slot = RECENT_COLORS_MAX - 1;
    } else if (slot == recent->count) {
        recent->count++;
    }
    for (int i = slot; i > 0; i--) {
        recent->colors[i] = recent->colors[i - 1];
    }
    recent->colors[0] = color;
}
