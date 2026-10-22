#include "tinysprite/slice_ops.hpp"

#include <utility>
#include <vector>

namespace
{

/* Records the change of the whole slice list from what it is now to `next` and applies it. */
bool replaceSlices(History &history, Sprite &sprite, const std::vector<Sprite::Slice> &next)
{
    std::vector<Sprite::Slice> before = sprite.slices();
    return history.perform(HistoryAction{[&sprite, before] { sprite.setSlices(before); },
                                         [&sprite, next] { sprite.setSlices(next); }});
}

bool same(const Sprite::Slice &a, const Sprite::Slice &b)
{
    return a.name == b.name && a.rect.x == b.rect.x && a.rect.y == b.rect.y &&
           a.rect.w == b.rect.w && a.rect.h == b.rect.h && a.hasPivot == b.hasPivot &&
           a.pivotX == b.pivotX && a.pivotY == b.pivotY;
}

bool indexValid(const Sprite &sprite, int index)
{
    return index >= 0 && index < static_cast<int>(sprite.slices().size());
}

} // namespace

bool slice_op_add(History &history, Sprite &sprite, Sprite::Slice slice)
{
    if (history.strokeActive() || !sprite.sliceValid(slice)) {
        return false;
    }
    slice.name = layer_name_truncate(slice.name, SLICE_NAME_MAX);
    std::vector<Sprite::Slice> next = sprite.slices();
    next.push_back(std::move(slice));
    return replaceSlices(history, sprite, next);
}

bool slice_op_set(History &history, Sprite &sprite, int index, Sprite::Slice slice)
{
    if (history.strokeActive() || !indexValid(sprite, index) || !sprite.sliceValid(slice)) {
        return false;
    }
    slice.name = layer_name_truncate(slice.name, SLICE_NAME_MAX);
    if (same(sprite.slices()[static_cast<size_t>(index)], slice)) {
        return true;
    }
    std::vector<Sprite::Slice> next = sprite.slices();
    next[static_cast<size_t>(index)] = std::move(slice);
    return replaceSlices(history, sprite, next);
}

bool slice_op_remove(History &history, Sprite &sprite, int index)
{
    if (history.strokeActive() || !indexValid(sprite, index)) {
        return false;
    }
    std::vector<Sprite::Slice> next = sprite.slices();
    next.erase(next.begin() + index);
    return replaceSlices(history, sprite, next);
}
