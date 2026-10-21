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
    if (!history.pushAction(HistoryAction{[&sprite, before] { sprite.palette() = before; },
                                          [&sprite, next] { sprite.palette() = next; }})) {
        return false;
    }
    sprite.palette() = std::move(next);
    return true;
}
