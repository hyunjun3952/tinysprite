#ifndef TINYSPRITE_IO_FRAME_RANGE_H
#define TINYSPRITE_IO_FRAME_RANGE_H

#include "tinysprite/sprite.h"

/* The last frame of an export that runs from `first` to `last` (negative: the sprite's last frame),
 * or -1 when the range does not fit the sprite. */
inline int frame_range_end(const Sprite &sprite, int first, int last)
{
    int end = last < 0 ? sprite.frameCount() - 1 : last;
    return first < 0 || end >= sprite.frameCount() || first > end ? -1 : end;
}

#endif
