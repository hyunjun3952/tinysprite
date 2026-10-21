#include "tinysprite/layer_ops.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "tinysprite/composite.h"

namespace
{

/* Ownership rule for every op below: a layer belongs to the sprite while it is attached and to
 * the step that detached it otherwise, so the history frees it only when it is dropped detached. */

/* Records `action` as one undo step; `applyNow` also runs its redo. */
bool record(History &history, HistoryAction action, bool applyNow)
{
    std::function<void()> redo = action.redo;
    if (!history.pushAction(std::move(action))) {
        return false;
    }
    if (applyNow && redo) {
        redo();
    }
    return true;
}

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
    record(history, layerAdded(m), false);
    return &layer;
}

bool layer_op_remove(History &history, Sprite &sprite, int index)
{
    if (history.strokeActive() || !sprite.layer(index) || sprite.layerCount() < 2) {
        return false;
    }
    auto m = std::make_shared<Membership>(Membership{sprite, sprite.layer(index), index, {}});
    return record(history, layerRemoved(m), true);
}

Layer *layer_op_duplicate(History &history, Sprite &sprite, int index)
{
    const Layer *source = sprite.layer(index);
    if (!source || history.strokeActive()) {
        return nullptr;
    }
    constexpr const char *SUFFIX = " copy";
    std::string name = source->name().substr(0, LAYER_NAME_MAX - 5) + SUFFIX;
    auto copy =
        std::make_unique<Layer>(name, sprite.width(), sprite.height(), source->frameCount());
    copy->setVisible(source->visible());
    copy->setOpacity(source->opacity());
    for (int frame = 0; frame < source->frameCount(); frame++) {
        const Cel *from = source->cel(frame);
        std::copy_n(from->pixels(), static_cast<size_t>(sprite.width()) * sprite.height(),
                    copy->cel(frame)->pixels());
    }
    Layer *result = copy.get();
    sprite.insertLayer(index + 1, std::move(copy));
    auto m = std::make_shared<Membership>(Membership{sprite, result, index + 1, {}});
    record(history, layerAdded(m), false);
    return result;
}

namespace
{

/* Merge down: the lower layer's cels before and after the merge, per frame, plus the upper layer.
 * Restoring whole cels keeps the pixel steps recorded earlier on either layer valid, since undo
 * returns them to exactly the state they saw. */
struct Merge {
    Membership upper;
    Layer *lower;
    size_t pixels;            /* per frame */
    std::vector<Rgba> before; /* frames * pixels */
    std::vector<Rgba> after;

    void write(const std::vector<Rgba> &snapshot)
    {
        for (int frame = 0; frame < lower->frameCount(); frame++) {
            auto first = snapshot.begin() + static_cast<std::ptrdiff_t>(frame * pixels);
            std::copy_n(first, pixels, lower->cel(frame)->pixels());
        }
    }
};

} // namespace

bool layer_op_merge_down(History &history, Sprite &sprite, int index)
{
    Layer *upper = sprite.layer(index);
    Layer *lower = sprite.layer(index - 1);
    if (!upper || !lower || history.strokeActive() || !upper->visible() || !lower->visible()) {
        return false;
    }
    auto m = std::make_shared<Merge>(Merge{Membership{sprite, upper, index, {}},
                                           lower,
                                           static_cast<size_t>(sprite.width()) * sprite.height(),
                                           {},
                                           {}});
    for (int frame = 0; frame < lower->frameCount(); frame++) {
        const Rgba *low = lower->cel(frame)->pixels();
        const Rgba *up = upper->cel(frame)->pixels();
        for (size_t i = 0; i < m->pixels; i++) {
            m->before.push_back(low[i]);
            m->after.push_back(composite_over(low[i], up[i], upper->opacity()));
        }
    }
    return record(history,
                  HistoryAction{[m] {
                                    m->write(m->before);
                                    m->upper.attach();
                                },
                                [m] {
                                    m->write(m->after);
                                    m->upper.detach();
                                }},
                  true);
}

bool layer_op_move(History &history, Sprite &sprite, int from, int to)
{
    if (history.strokeActive() || !sprite.layer(from) || !sprite.layer(to)) {
        return false;
    }
    if (from == to) {
        return true;
    }
    return record(history,
                  HistoryAction{[&sprite, from, to] { sprite.moveLayer(to, from); },
                                [&sprite, from, to] { sprite.moveLayer(from, to); }},
                  true);
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
    return record(history, HistoryAction{[set, old] { set(old); }, [set, next] { set(next); }},
                  applyNow);
}

/* Names compare after the same truncation Layer::setName applies, so the comparison and the undo
 * match what the layer holds. */
std::string truncated(const std::string &name)
{
    return name.substr(0, LAYER_NAME_MAX);
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

bool layer_op_set_opacity(History &history, Layer &layer, uint8_t opacity)
{
    return changeValue<uint8_t>(
        history, layer.opacity(), opacity, [&layer](uint8_t v) { layer.setOpacity(v); }, true);
}

bool layer_op_record_opacity(History &history, Layer &layer, uint8_t before)
{
    return changeValue<uint8_t>(
        history, before, layer.opacity(), [&layer](uint8_t v) { layer.setOpacity(v); }, false);
}
