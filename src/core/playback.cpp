#include "tinysprite/playback.h"

#include <algorithm>

std::vector<int> play_order(int first, int last, PlayMode mode)
{
    if (first > last) {
        std::swap(first, last);
    }
    std::vector<int> order;
    for (int f = first; f <= last; f++) {
        order.push_back(f);
    }
    if (mode == PlayMode::Reverse) {
        std::reverse(order.begin(), order.end());
    } else if (mode == PlayMode::PingPong) {
        for (int f = last - 1; f > first; f--) {
            order.push_back(f);
        }
    }
    return order;
}

void Playback::start(int frame, int first, int last, PlayMode mode)
{
    if (first > last) {
        std::swap(first, last);
    }
    first_ = first;
    last_ = last;
    frame_ = std::clamp(frame, first, last);
    mode_ = mode;
    direction_ = mode == PlayMode::Reverse ? -1 : 1;
    inFrame_ = 0;
    playing_ = true;
}

void Playback::stop()
{
    playing_ = false;
}

/* How long one pass over the frames first..last takes (out and back for ping-pong, the two ends
 * shown once). */
static long long loopLength(const Sprite &sprite, int first, int last, PlayMode mode)
{
    long long loop = 0;
    for (int f = first; f <= last; f++) {
        loop += sprite.frameDuration(f);
    }
    if (mode == PlayMode::PingPong) {
        loop = 2 * loop - sprite.frameDuration(first) - sprite.frameDuration(last);
    }
    return loop;
}

/* The frame after the current one; ping-pong turns around at an end, the other modes wrap. */
int Playback::nextFrame(int first, int last)
{
    int next = frame_ + direction_;
    if (next <= last && next >= first) {
        return next;
    }
    if (mode_ == PlayMode::PingPong) {
        direction_ = -direction_;
        return frame_ + direction_;
    }
    return direction_ > 0 ? first : last;
}

int Playback::advance(const Sprite &sprite, int elapsedMs)
{
    if (!playing_) {
        return -1;
    }
    int first = std::clamp(first_, 0, sprite.frameCount() - 1);
    int last = std::clamp(last_, first, sprite.frameCount() - 1);
    frame_ = std::clamp(frame_, first, last);
    if (first == last) {
        return frame_;
    }
    inFrame_ += std::max(elapsedMs, 0);
    /* A step is never shorter than one millisecond, so this ends; a long pause is first reduced to
     * whole loops of the range so it takes no longer than one. */
    long long loop = loopLength(sprite, first, last, mode_);
    inFrame_ = inFrame_ > 2 * loop ? inFrame_ % loop + loop : inFrame_;
    while (inFrame_ >= sprite.frameDuration(frame_)) {
        inFrame_ -= sprite.frameDuration(frame_);
        frame_ = nextFrame(first, last);
    }
    return frame_;
}
