#include "tinysprite/tag_ops.h"

#include <utility>
#include <vector>

namespace
{

/* Records the change of the whole tag list from what it is now to `next` and applies it. */
bool replaceTags(History &history, Sprite &sprite, const std::vector<Sprite::Tag> &next)
{
    std::vector<Sprite::Tag> before = sprite.tags();
    return history.perform(HistoryAction{[&sprite, before] { sprite.setTags(before); },
                                         [&sprite, next] { sprite.setTags(next); }});
}

bool same(const Sprite::Tag &a, const Sprite::Tag &b)
{
    return a.name == b.name && a.from == b.from && a.to == b.to && a.direction == b.direction;
}

} // namespace

bool tag_op_add(History &history, Sprite &sprite, Sprite::Tag tag)
{
    if (history.strokeActive() || !sprite.tagValid(tag)) {
        return false;
    }
    tag.name = layer_name_truncate(tag.name, TAG_NAME_MAX);
    std::vector<Sprite::Tag> next = sprite.tags();
    next.push_back(std::move(tag));
    return replaceTags(history, sprite, next);
}

bool tag_op_set(History &history, Sprite &sprite, int index, Sprite::Tag tag)
{
    if (history.strokeActive() || index < 0 || index >= static_cast<int>(sprite.tags().size()) ||
        !sprite.tagValid(tag)) {
        return false;
    }
    tag.name = layer_name_truncate(tag.name, TAG_NAME_MAX);
    if (same(sprite.tags()[static_cast<size_t>(index)], tag)) {
        return true;
    }
    std::vector<Sprite::Tag> next = sprite.tags();
    next[static_cast<size_t>(index)] = std::move(tag);
    return replaceTags(history, sprite, next);
}

bool tag_op_remove(History &history, Sprite &sprite, int index)
{
    if (history.strokeActive() || index < 0 || index >= static_cast<int>(sprite.tags().size())) {
        return false;
    }
    std::vector<Sprite::Tag> next = sprite.tags();
    next.erase(next.begin() + index);
    return replaceTags(history, sprite, next);
}
