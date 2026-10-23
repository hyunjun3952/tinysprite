/* Editor: the current frame, frame operations and playback. */

#include "editor.hpp"

#include <algorithm>
#include <string>

#include "editor/internal.hpp"
#include "tinysprite/cel_ops.hpp"
#include "tinysprite/frame_ops.hpp"
#include "tinysprite/tag_ops.hpp"

bool Editor::setFrame(int frame)
{
    if (stroking() || frame < 0 || frame >= sprite().frameCount()) {
        return false;
    }
    stopPlayback();
    if (frame != frame_) {
        frame_ = frame;
        markAllDirty();
    }
    return true;
}

/* A frame was refused: by the frame limit, else only the cel budget (or the allocator) does. */
void Editor::refusedFrame()
{
    if (sprite().frameCount() >= SPRITE_MAX_FRAMES) {
        framesFull_ = true;
    } else {
        memoryShort_ = true;
    }
}

void Editor::addFrame()
{
    stopPlayback();
    if (stroking()) {
        return;
    }
    bool added = false;
    guarded([&] { added = frame_op_add(history(), sprite(), frame_ + 1); });
    if (added) {
        frame_++;
        markAllDirty();
    } else {
        refusedFrame();
    }
}

void Editor::duplicateFrame()
{
    stopPlayback();
    if (stroking()) {
        return;
    }
    bool added = false;
    guarded([&] { added = frame_op_duplicate(history(), sprite(), frame_); });
    if (added) {
        frame_++;
        markAllDirty();
    } else {
        refusedFrame();
    }
}

void Editor::removeFrame(int index)
{
    stopPlayback();
    const std::vector<Sprite::Tag> tags = sprite().tags();
    if (stroking() || !frame_op_remove(history(), sprite(), index)) {
        return;
    }
    selectedTag_ = editor_detail::followTag(tags, sprite().tags(), selectedTag_);
    if (frame_ > index) {
        frame_--;
    }
    frame_ = std::clamp(frame_, 0, sprite().frameCount() - 1);
    markAllDirty();
}

void Editor::moveFrame(int from, int to)
{
    stopPlayback();
    if (stroking() || !frame_op_move(history(), sprite(), from, to)) {
        return;
    }
    if (frame_ == from) {
        frame_ = to;
    } else if (from < frame_ && frame_ <= to) {
        frame_--;
    } else if (to <= frame_ && frame_ < from) {
        frame_++;
    }
    markAllDirty();
}

void Editor::setFrameDuration(int index, int ms)
{
    if (!stroking()) {
        frame_op_set_duration(history(), sprite(), index, ms);
    }
}

void Editor::linkCel(int layer, int from, int to)
{
    stopPlayback();
    Layer *target = sprite().layer(layer);
    if (!stroking() && target && !target->locked() && cel_op_link(history(), *target, from, to)) {
        markAllDirty();
    }
}

void Editor::unlinkCel(int layer, int frame)
{
    stopPlayback();
    Layer *target = sprite().layer(layer);
    bool unlinked = false;
    if (!stroking() && target && !target->locked()) {
        guarded([&] { unlinked = cel_op_unlink(history(), *target, frame); });
    }
    if (unlinked) {
        markAllDirty();
    }
}

void Editor::linkCelToPrevious()
{
    linkCel(activeLayer(), frame_ - 1, frame_);
}

void Editor::togglePlayback()
{
    if (playing()) {
        stopPlayback();
    } else if (!stroking()) {
        startPlayback();
        markAllDirty(); /* the onion skin is hidden while playing */
    }
}

/* Runs over the selected tag, or all the frames. */
void Editor::startPlayback()
{
    if (selectedTag_ >= 0 && selectedTag_ < static_cast<int>(sprite().tags().size())) {
        const Sprite::Tag &tag = sprite().tags()[static_cast<size_t>(selectedTag_)];
        playback_.start(frame_, tag.from, tag.to, tag.direction);
    } else {
        playback_.start(frame_, 0, sprite().frameCount() - 1, playMode_);
    }
    frame_ = playback_.frame(); /* a start frame outside the range is pulled into it */
}

void Editor::selectTag(int index)
{
    selectedTag_ = index >= 0 && index < static_cast<int>(sprite().tags().size()) ? index : -1;
    if (playing()) {
        startPlayback();
    }
}

void Editor::addTag()
{
    stopPlayback();
    if (stroking()) {
        return;
    }
    Sprite::Tag tag;
    tag.name = "Tag " + std::to_string(sprite().tags().size() + 1);
    tag.from = frame_;
    tag.to = frame_;
    if (tag_op_add(history(), sprite(), tag)) {
        selectedTag_ = static_cast<int>(sprite().tags().size()) - 1;
    }
}

void Editor::editTag(int index, const Sprite::Tag &tag)
{
    if (!stroking()) {
        tag_op_set(history(), sprite(), index, tag);
        if (playing()) {
            startPlayback();
        }
    }
}

void Editor::removeTag(int index)
{
    if (stroking() || !tag_op_remove(history(), sprite(), index)) {
        return;
    }
    if (selectedTag_ == index) {
        selectedTag_ = -1;
    } else if (selectedTag_ > index) {
        selectedTag_--;
    }
}

void Editor::stopPlayback()
{
    if (playing()) {
        playback_.stop();
        markAllDirty();
    }
}

void Editor::setOnion(bool enabled, OnionSkin skin)
{
    skin.before = std::clamp(skin.before, 0, ONION_MAX_FRAMES);
    skin.after = std::clamp(skin.after, 0, ONION_MAX_FRAMES);
    onionOn_ = enabled;
    onion_ = skin;
    markAllDirty();
}

void Editor::setPlayMode(PlayMode mode)
{
    playMode_ = mode;
    if (playing()) {
        startPlayback();
    }
}

void Editor::tick(int elapsedMs)
{
    if (!playing() || stroking()) {
        return;
    }
    int frame = playback_.advance(sprite(), elapsedMs);
    if (frame >= 0 && frame != frame_) {
        frame_ = frame;
        markAllDirty();
    }
}
