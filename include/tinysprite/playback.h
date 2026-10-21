#ifndef TINYSPRITE_PLAYBACK_H
#define TINYSPRITE_PLAYBACK_H

#include "tinysprite/sprite.h"

/* Which frame an animation shows as time passes. It owns no clock: the caller reports how many
 * milliseconds went by. Playback runs inside a frame range and loops; a ping-pong turns at the
 * ends without showing them twice. */
class Playback
{
  public:
    /* Starts at `frame` within [first, last] (a frame outside the range is pulled into it; a
     * reversed range is swapped). */
    void start(int frame, int first, int last, PlayMode mode);
    void stop();

    bool playing() const
    {
        return playing_;
    }
    int frame() const
    {
        return frame_;
    }

    /* Moves on by `elapsedMs` using the frames' durations and returns the frame to show, or -1 when
     * not playing. The range is clipped to the sprite's frames. */
    int advance(const Sprite &sprite, int elapsedMs);

  private:
    bool playing_ = false;
    int frame_ = 0;
    int first_ = 0;
    int last_ = 0;
    PlayMode mode_ = PlayMode::Forward;
    int direction_ = 1;     /* +1 or -1 */
    long long inFrame_ = 0; /* milliseconds spent on the current frame */
};

#endif
