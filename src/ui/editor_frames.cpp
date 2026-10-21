/* Editor: the current frame, frame operations and playback. */

#include "editor.h"

#include <algorithm>

#include "tinysprite/frame_ops.h"

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

void Editor::addFrame()
{
    stopPlayback();
    if (!stroking() && frame_op_add(history(), sprite(), frame_ + 1)) {
        frame_++;
        markAllDirty();
    }
}

void Editor::duplicateFrame()
{
    stopPlayback();
    if (!stroking() && frame_op_duplicate(history(), sprite(), frame_)) {
        frame_++;
        markAllDirty();
    }
}

void Editor::removeFrame(int index)
{
    stopPlayback();
    if (stroking() || !frame_op_remove(history(), sprite(), index)) {
        return;
    }
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

void Editor::togglePlayback()
{
    if (playing()) {
        stopPlayback();
    } else if (!stroking()) {
        playback_.start(frame_, 0, sprite().frameCount() - 1, playMode_);
    }
}

void Editor::stopPlayback()
{
    playback_.stop();
}

void Editor::setPlayMode(PlayMode mode)
{
    playMode_ = mode;
    if (playing()) {
        playback_.start(frame_, 0, sprite().frameCount() - 1, playMode_);
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
