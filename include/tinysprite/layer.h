#ifndef TINYSPRITE_LAYER_H
#define TINYSPRITE_LAYER_H

#include "tinysprite/cel.h"

#define LAYER_NAME_MAX 63

typedef struct Layer Layer;

/* Creates a visible, fully opaque layer holding one blank cel per frame.
 * A NULL name becomes ""; names longer than LAYER_NAME_MAX are truncated.
 * Returns NULL on invalid arguments or allocation failure. */
Layer *layer_create(const char *name, int width, int height, int frame_count);
void layer_destroy(Layer *layer);

const char *layer_name(const Layer *layer);

/* A NULL name becomes ""; names longer than LAYER_NAME_MAX are truncated. */
void layer_set_name(Layer *layer, const char *name);

bool layer_visible(const Layer *layer);
void layer_set_visible(Layer *layer, bool visible);

uint8_t layer_opacity(const Layer *layer);
void layer_set_opacity(Layer *layer, uint8_t opacity);

int layer_frame_count(const Layer *layer);

/* Returns NULL if frame is out of range. */
Cel *layer_cel(Layer *layer, int frame);

#endif
