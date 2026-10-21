#ifndef TINYSPRITE_CLIP_H
#define TINYSPRITE_CLIP_H

#include "tinysprite/history.h"
#include "tinysprite/rect.h"

/* A rectangle of pixels copied out of a cel: the clipboard behind copy / cut / paste, and the
 * scratch buffer for flipping and rotating a selection. Independent of the cel it came from. */
typedef struct Clip Clip;

/* Copies the pixels of the rectangle, clipped to the cel. Returns NULL if cel is NULL, the clipped
 * rectangle is empty, or on allocation failure. */
Clip *clip_copy(const Cel *cel, Rect area);
void clip_destroy(Clip *clip);

int clip_width(const Clip *clip);
int clip_height(const Clip *clip);

/* Returns transparent black for out-of-bounds coordinates. */
Rgba clip_get(const Clip *clip, int x, int y);

void clip_flip_horizontal(Clip *clip);
void clip_flip_vertical(Clip *clip);

/* Turns the clip by 90 degrees (clockwise when `clockwise` is non-zero), swapping its width and
 * height. Returns 0 on success, -1 if clip is NULL or on allocation failure (clip is unchanged). */
int clip_rotate(Clip *clip, bool clockwise);

/* Writes the clip with its top-left corner at (x, y) as one undo step: opaque pixels land over
 * whatever is there, transparent ones leave the destination alone, and pixels outside the cel are
 * skipped. Returns 0 on success (including a paste that changed nothing), -1 on invalid arguments,
 * an active stroke, or allocation failure. */
int clip_paste(History *history, Cel *cel, const Clip *clip, int x, int y);

/* Clears `cleared` and then pastes the clip with its top-left corner at (x, y), all in one undo
 * step. This is how a selection is flipped or rotated in place. Same return values as
 * clip_paste. */
int clip_replace(History *history, Cel *cel, Rect cleared, const Clip *clip, int x, int y);

#endif
