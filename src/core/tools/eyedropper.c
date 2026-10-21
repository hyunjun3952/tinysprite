#include "tinysprite/eyedropper.h"

#include "tinysprite/composite.h"

int eyedropper_pick_cel(const Cel *cel, int x, int y, Rgba *out)
{
    if (!cel || !out || !cel_in_bounds(cel, x, y)) {
        return -1;
    }
    *out = cel_get(cel, x, y);
    return 0;
}

int eyedropper_pick_sprite(Sprite *sprite, int frame, int x, int y, Rgba *out)
{
    if (!sprite || !out || frame < 0 || frame >= sprite_frame_count(sprite) || x < 0 || y < 0 ||
        x >= sprite_width(sprite) || y >= sprite_height(sprite)) {
        return -1;
    }
    *out = composite_pixel(sprite, frame, x, y);
    return 0;
}
