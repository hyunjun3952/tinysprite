#include "tinysprite/palette_ops.h"

#include <utility>

bool palette_op_set(History &history, Sprite &sprite, Palette next)
{
    if (history.strokeActive()) {
        return false;
    }
    if (sprite.palette() == next) {
        return true;
    }
    Palette before = sprite.palette();
    return history.perform(
        HistoryAction{[&sprite, before] { sprite.palette() = before; },
                      [&sprite, next = std::move(next)] { sprite.palette() = next; }});
}
