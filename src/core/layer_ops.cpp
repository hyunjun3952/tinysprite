#include "tinysprite/layer_ops.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "tinysprite/composite.hpp"

namespace
{

/* Ownership rule for every op below: a layer belongs to the sprite while it is attached and to
 * the step that detached it otherwise, so the history frees it only when it is dropped detached. */

/* Add / remove / merge: where a layer sits in its sprite, and who owns it while it does not. */
struct Membership {
    Sprite &sprite;
    Layer *layer;
    int index;
    std::unique_ptr<Layer> detached; /* set exactly while the layer is outside the sprite */

    void attach()
    {
        if (detached && sprite.insertLayer(index, std::move(detached))) {
            detached = nullptr;
        }
    }
    void detach()
    {
        if (!detached) {
            detached = sprite.detachLayer(index);
        }
    }
};

HistoryAction layerAdded(const std::shared_ptr<Membership> &m)
{
    return HistoryAction{[m] { m->detach(); }, [m] { m->attach(); }};
}

HistoryAction layerRemoved(const std::shared_ptr<Membership> &m)
{
    return HistoryAction{[m] { m->attach(); }, [m] { m->detach(); }};
}

} // namespace

Layer *layer_op_add(History &history, Sprite &sprite, const std::string &name)
{
    if (history.strokeActive()) {
        return nullptr;
    }
    Layer &layer = sprite.addLayer(name);
    auto m = std::make_shared<Membership>(Membership{sprite, &layer, sprite.layerCount() - 1, {}});
    history.pushAction(layerAdded(m));
    return &layer;
}

bool layer_op_remove(History &history, Sprite &sprite, int index)
{
    if (history.strokeActive() || !sprite.layer(index) || sprite.layerCount() < 2) {
        return false;
    }
    auto m = std::make_shared<Membership>(Membership{sprite, sprite.layer(index), index, {}});
    return history.perform(layerRemoved(m));
}

Layer *layer_op_duplicate(History &history, Sprite &sprite, int index)
{
    const Layer *source = sprite.layer(index);
    if (!source || history.strokeActive()) {
        return nullptr;
    }
    constexpr const char *SUFFIX = " copy";
    std::string name = layer_name_truncate(source->name(), LAYER_NAME_MAX - 5) + SUFFIX;
    auto copy =
        std::make_unique<Layer>(name, sprite.width(), sprite.height(), source->frameCount());
    copy->setVisible(source->visible());
    copy->setOpacity(source->opacity());
    copy->setBlendMode(source->blendMode());
    /* Frames that share a cel in the source share the copy of it too. */
    std::vector<std::shared_ptr<Cel>> cels;
    for (int frame = 0; frame < source->frameCount(); frame++) {
        const Cel *from = source->cel(frame);
        int first = 0;
        while (source->cel(first) != from) {
            first++;
        }
        if (first < frame) {
            cels.push_back(cels[static_cast<size_t>(first)]);
            continue;
        }
        std::copy_n(from->pixels(), static_cast<size_t>(sprite.width()) * sprite.height(),
                    copy->cel(frame)->pixels());
        cels.push_back(copy->celList()[static_cast<size_t>(frame)]);
    }
    copy->setCelList(std::move(cels));
    Layer *result = copy.get();
    sprite.insertLayer(index + 1, std::move(copy));
    auto m = std::make_shared<Membership>(Membership{sprite, result, index + 1, {}});
    history.pushAction(layerAdded(m));
    return result;
}

namespace
{

/* Merge down: the lower layer's cels before and after the merge, per frame, plus the upper layer.
 * Restoring whole cels keeps the pixel steps recorded earlier on either layer valid, since undo
 * returns them to exactly the state they saw. Frames of the lower layer that shared a cel get a
 * cel each for the merge (the upper layer differs per frame), and undo links them again. */
struct Merge {
    Membership upper;
    Layer *lower;
    size_t pixels;            /* per frame */
    std::vector<Rgba> before; /* frames * pixels */
    std::vector<Rgba> after;
    std::vector<std::shared_ptr<Cel>> linked;   /* the lower layer's cels as they were */
    std::vector<std::shared_ptr<Cel>> separate; /* ... and with every frame holding its own */

    void write(const std::vector<Rgba> &snapshot) const
    {
        for (int frame = 0; frame < lower->frameCount(); frame++) {
            auto first = snapshot.begin() + static_cast<std::ptrdiff_t>(frame * pixels);
            std::copy_n(first, pixels, lower->cel(frame)->pixels());
        }
    }
};

} // namespace

bool layer_can_merge(const Layer &upper, const Layer &lower)
{
    return upper.visible() && lower.visible() && !lower.locked();
}

bool layer_merge_keeps_look(const Layer &upper, const Layer &lower, bool visibleBelow)
{
    bool plain = upper.blendMode() == BlendMode::Normal && lower.blendMode() == BlendMode::Normal;
    return plain || !visibleBelow;
}

bool layer_any_visible_below(const Sprite &sprite, int index)
{
    for (int i = 0; i < index; i++) {
        if (sprite.layer(i)->visible()) {
            return true;
        }
    }
    return false;
}

bool layer_can_merge_down(const Sprite &sprite, int index)
{
    const Layer *upper = sprite.layer(index);
    const Layer *lower = sprite.layer(index - 1);
    if (!upper || !lower || !layer_can_merge(*upper, *lower)) {
        return false;
    }
    return layer_merge_keeps_look(*upper, *lower, layer_any_visible_below(sprite, index - 1));
}

bool layer_op_merge_down(History &history, Sprite &sprite, int index)
{
    if (history.strokeActive() || !layer_can_merge_down(sprite, index)) {
        return false;
    }
    Layer *upper = sprite.layer(index);
    Layer *lower = sprite.layer(index - 1);
    auto m = std::make_shared<Merge>(Merge{Membership{sprite, upper, index, {}},
                                           lower,
                                           static_cast<size_t>(sprite.width()) * sprite.height(),
                                           {},
                                           {},
                                           lower->celList(),
                                           {}});
    m->separate = m->linked;
    for (size_t frame = 1; frame < m->separate.size(); frame++) {
        const std::shared_ptr<Cel> &cel = m->linked[frame];
        if (std::find(m->linked.begin(), m->linked.begin() + static_cast<std::ptrdiff_t>(frame),
                      cel) != m->linked.begin() + static_cast<std::ptrdiff_t>(frame)) {
            auto own = std::make_shared<Cel>(cel->width(), cel->height());
            std::copy_n(cel->pixels(), m->pixels, own->pixels());
            m->separate[frame] = std::move(own);
        }
    }
    for (int frame = 0; frame < lower->frameCount(); frame++) {
        const Rgba *low = lower->cel(frame)->pixels();
        const Rgba *up = upper->cel(frame)->pixels();
        for (size_t i = 0; i < m->pixels; i++) {
            m->before.push_back(low[i]);
            m->after.push_back(
                composite_blend(low[i], up[i], upper->opacity(), upper->blendMode()));
        }
    }
    return history.perform(HistoryAction{[m] {
                                             m->lower->setCelList(m->linked);
                                             m->write(m->before);
                                             m->upper.attach();
                                         },
                                         [m] {
                                             m->lower->setCelList(m->separate);
                                             m->write(m->after);
                                             m->upper.detach();
                                         }});
}

bool layer_op_move(History &history, Sprite &sprite, int from, int to)
{
    if (history.strokeActive() || !sprite.layer(from) || !sprite.layer(to)) {
        return false;
    }
    if (from == to) {
        return true;
    }
    return history.perform(HistoryAction{[&sprite, from, to] { sprite.moveLayer(to, from); },
                                         [&sprite, from, to] { sprite.moveLayer(from, to); }});
}

namespace
{

/* Name, visibility and opacity are each a value behind a setter; records old -> new. `applyNow` is
 * false when the layer already holds `next`. */
template <class T>
bool changeValue(History &history, const T &old, const T &next,
                 const std::function<void(const T &)> &set, bool applyNow)
{
    if (history.strokeActive()) {
        return false;
    }
    if (old == next) {
        return true;
    }
    HistoryAction action{[set, old] { set(old); }, [set, next] { set(next); }};
    return applyNow ? history.perform(std::move(action)) : history.pushAction(std::move(action));
}

/* Names compare after the same truncation Layer::setName applies, so the comparison and the undo
 * match what the layer holds. */
std::string truncated(const std::string &name)
{
    return layer_name_truncate(name);
}

auto nameSetter(Layer &layer)
{
    return std::function<void(const std::string &)>(
        [&layer](const std::string &v) { layer.setName(v); });
}

} // namespace

bool layer_op_set_name(History &history, Layer &layer, const std::string &name)
{
    return changeValue(history, layer.name(), truncated(name), nameSetter(layer), true);
}

bool layer_op_record_name(History &history, Layer &layer, const std::string &before)
{
    return changeValue(history, truncated(before), layer.name(), nameSetter(layer), false);
}

bool layer_op_set_visible(History &history, Layer &layer, bool visible)
{
    return changeValue<bool>(
        history, layer.visible(), visible, [&layer](bool v) { layer.setVisible(v); }, true);
}

bool layer_op_set_locked(History &history, Layer &layer, bool locked)
{
    return changeValue<bool>(
        history, layer.locked(), locked, [&layer](bool v) { layer.setLocked(v); }, true);
}

bool layer_op_set_opacity(History &history, Layer &layer, uint8_t opacity)
{
    return changeValue<uint8_t>(
        history, layer.opacity(), opacity, [&layer](uint8_t v) { layer.setOpacity(v); }, true);
}

bool layer_op_set_blend_mode(History &history, Layer &layer, BlendMode mode)
{
    return changeValue<BlendMode>(
        history, layer.blendMode(), mode, [&layer](BlendMode v) { layer.setBlendMode(v); }, true);
}

bool layer_op_record_opacity(History &history, Layer &layer, uint8_t before)
{
    return changeValue<uint8_t>(
        history, before, layer.opacity(), [&layer](uint8_t v) { layer.setOpacity(v); }, false);
}
