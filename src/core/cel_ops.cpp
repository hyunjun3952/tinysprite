#include "tinysprite/cel_ops.hpp"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace
{

using CelList = std::vector<std::shared_ptr<Cel>>;

/* Records the swap of the layer's cels from what they are to `next` and applies it. */
bool replaceCels(History &history, Layer &layer, const CelList &next)
{
    CelList before = layer.celList();
    return history.perform(HistoryAction{[&layer, before] { layer.setCelList(before); },
                                         [&layer, next] { layer.setCelList(next); }});
}

} // namespace

bool cel_op_link(History &history, Layer &layer, int from, int to)
{
    if (history.strokeActive() || !layer.cel(from) || !layer.cel(to)) {
        return false;
    }
    if (layer.cel(from) == layer.cel(to)) {
        return true;
    }
    CelList next = layer.celList();
    next[static_cast<size_t>(to)] = next[static_cast<size_t>(from)];
    return replaceCels(history, layer, next);
}

bool cel_op_unlink(History &history, Layer &layer, int frame)
{
    if (history.strokeActive() || !layer.cel(frame)) {
        return false;
    }
    if (!layer.celLinked(frame)) {
        return true;
    }
    const Cel &source = *layer.cel(frame);
    auto copy = std::make_shared<Cel>(source.width(), source.height());
    std::copy_n(source.pixels(), static_cast<size_t>(source.width()) * source.height(),
                copy->pixels());
    CelList next = layer.celList();
    next[static_cast<size_t>(frame)] = std::move(copy);
    return replaceCels(history, layer, next);
}
