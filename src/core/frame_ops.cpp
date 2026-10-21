#include "tinysprite/frame_ops.h"

#include <memory>
#include <optional>
#include <utility>

namespace
{

/* Where a frame sits in its sprite, and who owns it while it does not: the frame belongs to the
 * sprite while it is attached and to the history step that detached it otherwise. */
struct Slot {
    Sprite &sprite;
    int index;
    std::optional<Sprite::Frame> detached; /* set exactly while the frame is outside the sprite */

    void attach()
    {
        if (detached && sprite.insertFrame(index, std::move(*detached))) {
            detached.reset();
        }
    }
    void detach()
    {
        if (!detached) {
            detached = sprite.detachFrame(index);
        }
    }
};

/* Records an insertion of `frame` at `index` as one step and performs it. */
bool recordInsert(History &history, Sprite &sprite, int index, Sprite::Frame &&frame)
{
    auto slot = std::make_shared<Slot>(Slot{sprite, index, std::move(frame)});
    if (!history.pushAction(
            HistoryAction{[slot] { slot->detach(); }, [slot] { slot->attach(); }})) {
        return false;
    }
    slot->attach();
    return true;
}

} // namespace

bool frame_op_add(History &history, Sprite &sprite, int index)
{
    if (history.strokeActive() || index < 0 || index > sprite.frameCount()) {
        return false;
    }
    return recordInsert(history, sprite, index, sprite.makeFrame());
}

bool frame_op_duplicate(History &history, Sprite &sprite, int index)
{
    if (history.strokeActive() || index < 0 || index >= sprite.frameCount()) {
        return false;
    }
    return recordInsert(history, sprite, index + 1, sprite.copyFrame(index));
}

bool frame_op_remove(History &history, Sprite &sprite, int index)
{
    if (history.strokeActive() || index < 0 || index >= sprite.frameCount() ||
        sprite.frameCount() < 2) {
        return false;
    }
    auto slot = std::make_shared<Slot>(Slot{sprite, index, std::nullopt});
    if (!history.pushAction(
            HistoryAction{[slot] { slot->attach(); }, [slot] { slot->detach(); }})) {
        return false;
    }
    slot->detach();
    return true;
}

bool frame_op_move(History &history, Sprite &sprite, int from, int to)
{
    if (history.strokeActive() || from < 0 || from >= sprite.frameCount() || to < 0 ||
        to >= sprite.frameCount()) {
        return false;
    }
    if (from == to) {
        return true;
    }
    if (!history.pushAction(HistoryAction{[&sprite, from, to] { sprite.moveFrame(to, from); },
                                          [&sprite, from, to] { sprite.moveFrame(from, to); }})) {
        return false;
    }
    sprite.moveFrame(from, to);
    return true;
}

bool frame_op_set_duration(History &history, Sprite &sprite, int frame, int ms)
{
    if (history.strokeActive() || frame < 0 || frame >= sprite.frameCount() || ms < 1 ||
        ms > FRAME_DURATION_MAX) {
        return false;
    }
    int old = sprite.frameDuration(frame);
    if (old == ms) {
        return true;
    }
    if (!history.pushAction(
            HistoryAction{[&sprite, frame, old] { sprite.setFrameDuration(frame, old); },
                          [&sprite, frame, ms] { sprite.setFrameDuration(frame, ms); }})) {
        return false;
    }
    sprite.setFrameDuration(frame, ms);
    return true;
}
