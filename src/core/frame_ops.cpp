#include "tinysprite/frame_ops.hpp"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

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

/* The tags before and after a frame step. Sprite keeps tags in step when frames come and go, but a
 * tag that vanished with its last frame only comes back by restoring the list. */
struct TagSnapshots {
    std::vector<Sprite::Tag> before;
    std::vector<Sprite::Tag> after;
};

/* Records `slot` joining (inserting) or leaving its sprite as one step and performs it. */
bool recordSlot(History &history, Sprite &sprite, const std::shared_ptr<Slot> &slot, bool inserting)
{
    auto tags = std::make_shared<TagSnapshots>();
    tags->before = sprite.tags();
    auto attach = [slot, &sprite, tags](bool undoing) {
        slot->attach();
        sprite.setTags(undoing ? tags->before : tags->after);
    };
    auto detach = [slot, &sprite, tags](bool undoing) {
        slot->detach();
        sprite.setTags(undoing ? tags->before : tags->after);
    };
    HistoryAction action;
    if (inserting) {
        action.undo = [detach] { detach(true); };
        action.redo = [attach] { attach(false); };
    } else {
        action.undo = [attach] { attach(true); };
        action.redo = [detach] { detach(false); };
    }
    if (!history.pushAction(std::move(action))) {
        return false;
    }
    if (inserting) {
        slot->attach();
    } else {
        slot->detach();
    }
    tags->after = sprite.tags(); /* what redo restores */
    return true;
}

/* Records an insertion of `frame` at `index` as one step and performs it. */
bool recordInsert(History &history, Sprite &sprite, int index, Sprite::Frame &&frame)
{
    return recordSlot(history, sprite,
                      std::make_shared<Slot>(Slot{sprite, index, std::move(frame)}), true);
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
    return recordSlot(history, sprite, std::make_shared<Slot>(Slot{sprite, index, std::nullopt}),
                      false);
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
    return history.perform(HistoryAction{[&sprite, from, to] { sprite.moveFrame(to, from); },
                                         [&sprite, from, to] { sprite.moveFrame(from, to); }});
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
    return history.perform(
        HistoryAction{[&sprite, frame, old] { sprite.setFrameDuration(frame, old); },
                      [&sprite, frame, ms] { sprite.setFrameDuration(frame, ms); }});
}
