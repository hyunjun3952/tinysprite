#include "tinysprite/sprite.h"

#include <stdlib.h>

struct Sprite {
    int width;
    int height;
    int frame_count;
    Palette *palette;
    Layer **layers;
    int layer_count;
    int layer_capacity;
};

Sprite *sprite_create(int width, int height)
{
    if (width < 1 || width > CEL_MAX_DIMENSION || height < 1 || height > CEL_MAX_DIMENSION) {
        return NULL;
    }
    Sprite *sprite = calloc(1, sizeof(Sprite));
    if (sprite == NULL) {
        return NULL;
    }
    sprite->width = width;
    sprite->height = height;
    sprite->frame_count = 1;
    sprite->palette = palette_create();
    if (sprite->palette == NULL || sprite_add_layer(sprite, "Layer 1") == NULL) {
        sprite_destroy(sprite);
        return NULL;
    }
    return sprite;
}

void sprite_destroy(Sprite *sprite)
{
    if (sprite == NULL) {
        return;
    }
    for (int i = 0; i < sprite->layer_count; i++) {
        layer_destroy(sprite->layers[i]);
    }
    free(sprite->layers);
    palette_destroy(sprite->palette);
    free(sprite);
}

int sprite_width(const Sprite *sprite)
{
    return sprite->width;
}

int sprite_height(const Sprite *sprite)
{
    return sprite->height;
}

int sprite_frame_count(const Sprite *sprite)
{
    return sprite->frame_count;
}

Palette *sprite_palette(Sprite *sprite)
{
    return sprite->palette;
}

int sprite_layer_count(const Sprite *sprite)
{
    return sprite->layer_count;
}

Layer *sprite_layer(Sprite *sprite, int index)
{
    if (index < 0 || index >= sprite->layer_count) {
        return NULL;
    }
    return sprite->layers[index];
}

Layer *sprite_add_layer(Sprite *sprite, const char *name)
{
    if (sprite->layer_count == sprite->layer_capacity) {
        int capacity = sprite->layer_capacity == 0 ? 4 : sprite->layer_capacity * 2;
        Layer **grown = realloc(sprite->layers, (size_t)capacity * sizeof(Layer *));
        if (grown == NULL) {
            return NULL;
        }
        sprite->layers = grown;
        sprite->layer_capacity = capacity;
    }
    Layer *layer = layer_create(name, sprite->width, sprite->height, sprite->frame_count);
    if (layer == NULL) {
        return NULL;
    }
    sprite->layers[sprite->layer_count++] = layer;
    return layer;
}
