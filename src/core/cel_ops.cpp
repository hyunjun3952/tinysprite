#include "tinysprite/cel_ops.hpp"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace
{

using CelList = std::vector<std::shared_ptr<Cel>>;

/* The cel at `frame`, or null when the layer cannot be edited: a stroke is active or the frame is
 * out of range. */
Cel *editableCel(const History &history, Layer &layer, int frame)
{
    return history.strokeActive() ? nullptr : layer.cel(frame);
}

/* Makes `frame` show `cel`, dropping what it held, and records the swap as one undo step. */
bool replaceCel(History &history, Layer &layer, int frame, std::shared_ptr<Cel> cel)
{
    CelList before = layer.celList();
    CelList next = before;
    next[static_cast<size_t>(frame)] = std::move(cel);
    return history.perform(HistoryAction{[&layer, before] { layer.setCelList(before); },
                                         [&layer, next] { layer.setCelList(next); }});
}

} // namespace

bool cel_op_link(History &history, Layer &layer, int from, int to)
{
    const Cel *source = editableCel(history, layer, from);
    const Cel *target = editableCel(history, layer, to);
    if (!source || !target) {
        return false;
    }
    if (source == target) {
        return true;
    }
    return replaceCel(history, layer, to, layer.celList()[static_cast<size_t>(from)]);
}

bool cel_op_unlink(History &history, Layer &layer, int frame)
{
    const Cel *source = editableCel(history, layer, frame);
    if (!source) {
        return false;
    }
    if (!layer.celLinked(frame)) {
        return true;
    }
    auto copy = std::make_shared<Cel>(source->width(), source->height());
    std::copy_n(source->pixels(), static_cast<size_t>(source->width()) * source->height(),
                copy->pixels());
    return replaceCel(history, layer, frame, std::move(copy));
}
