#include "tinysprite/guide_ops.hpp"

namespace
{

bool same(const Guides &a, const Guides &b)
{
    return a.vertical == b.vertical && a.horizontal == b.horizontal;
}

} // namespace

bool guide_op_set(History &history, Sprite &sprite, const Guides &next)
{
    if (history.strokeActive()) {
        return false;
    }
    if (same(sprite.guides(), next)) {
        return true;
    }
    const Guides before = sprite.guides();
    return history.perform(HistoryAction{[&sprite, before] { sprite.guides() = before; },
                                         [&sprite, next] { sprite.guides() = next; }});
}
