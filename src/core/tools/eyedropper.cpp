#include "tinysprite/tools.hpp"

#include "tinysprite/composite.hpp"

std::optional<Rgba> eyedropper_pick_cel(const Cel &cel, int x, int y)
{
    if (!cel.inBounds(x, y)) {
        return std::nullopt;
    }
    return cel.get(x, y);
}

std::optional<Rgba> eyedropper_pick_sprite(const Sprite &sprite, int frame, int x, int y)
{
    if (frame < 0 || frame >= sprite.frameCount() || x < 0 || y < 0 || x >= sprite.width() ||
        y >= sprite.height()) {
        return std::nullopt;
    }
    return composite_pixel(sprite, frame, x, y);
}
