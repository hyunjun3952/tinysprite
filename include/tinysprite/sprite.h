#ifndef TINYSPRITE_SPRITE_H
#define TINYSPRITE_SPRITE_H

#include "tinysprite/layer.h"
#include "tinysprite/palette.h"

typedef struct Sprite Sprite;

/* Creates a sprite with one frame, an empty palette and one layer named "Layer 1".
 * Returns NULL if a dimension is outside 1..CEL_MAX_DIMENSION or on allocation failure. */
Sprite *sprite_create(int width, int height);
void sprite_destroy(Sprite *sprite);

int sprite_width(const Sprite *sprite);
int sprite_height(const Sprite *sprite);
int sprite_frame_count(const Sprite *sprite);

Palette *sprite_palette(Sprite *sprite);

int sprite_layer_count(const Sprite *sprite);

/* Layers are ordered bottom (index 0) to top. Returns NULL if index is out of range. */
Layer *sprite_layer(Sprite *sprite, int index);

/* Appends a new layer on top and returns it, or NULL on allocation failure. */
Layer *sprite_add_layer(Sprite *sprite, const char *name);

#endif
