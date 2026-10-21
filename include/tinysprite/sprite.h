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

/* Destroys the layer at `index`; the layers above it shift down. A sprite always keeps at least one
 * layer. Returns 0 on success, -1 if index is out of range or only one layer is left. The layer's
 * cels are freed, so callers must drop them from any history first (history_forget_cel). */
int sprite_remove_layer(Sprite *sprite, int index);

/* Moves the layer at `from` so it ends up at index `to`, shifting the layers in between. Returns 0
 * on success, -1 if either index is out of range. */
int sprite_move_layer(Sprite *sprite, int from, int to);

#endif
