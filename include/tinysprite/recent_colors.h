#ifndef TINYSPRITE_RECENT_COLORS_H
#define TINYSPRITE_RECENT_COLORS_H

#include "tinysprite/palette.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RECENT_COLORS_MAX 16

/* The colors drawn with most recently, newest first, without repeats. A plain value: zero it to
 * start empty. */
typedef struct RecentColors {
    Rgba colors[RECENT_COLORS_MAX];
    int count;
} RecentColors;

/* Puts `color` first. A color already in the list moves up instead of appearing twice, and the
 * oldest one drops off when the list is full. Does nothing if recent is NULL. */
void recent_colors_push(RecentColors *recent, Rgba color);

#ifdef __cplusplus
}
#endif

#endif
