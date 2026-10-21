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
    free((void *)
             sprite->layers); /* explicit cast: bugprone-multi-level-implicit-pointer-conversion */
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

static int reserve_layer_slot(Sprite *sprite)
{
    if (sprite->layer_count < sprite->layer_capacity) {
        return 0;
    }
    int capacity = sprite->layer_capacity == 0 ? 4 : sprite->layer_capacity * 2;
    Layer **grown = (Layer **)realloc((void *)sprite->layers, (size_t)capacity * sizeof(Layer *));
    if (grown == NULL) {
        return -1;
    }
    sprite->layers = grown;
    sprite->layer_capacity = capacity;
    return 0;
}

Layer *sprite_add_layer(Sprite *sprite, const char *name)
{
    if (reserve_layer_slot(sprite) != 0) {
        return NULL;
    }
    Layer *layer = layer_create(name, sprite->width, sprite->height, sprite->frame_count);
    if (layer == NULL) {
        return NULL;
    }
    sprite->layers[sprite->layer_count++] = layer;
    return layer;
}

Layer *sprite_detach_layer(Sprite *sprite, int index)
{
    if (index < 0 || index >= sprite->layer_count || sprite->layer_count == 1) {
        return NULL;
    }
    Layer *layer = sprite->layers[index];
    for (int i = index + 1; i < sprite->layer_count; i++) {
        sprite->layers[i - 1] = sprite->layers[i];
    }
    sprite->layer_count--;
    return layer;
}

int sprite_insert_layer(Sprite *sprite, int index, Layer *layer)
{
    if (layer == NULL || index < 0 || index > sprite->layer_count ||
        reserve_layer_slot(sprite) != 0) {
        return -1;
    }
    for (int i = sprite->layer_count; i > index; i--) {
        sprite->layers[i] = sprite->layers[i - 1];
    }
    sprite->layers[index] = layer;
    sprite->layer_count++;
    return 0;
}

int sprite_remove_layer(Sprite *sprite, int index)
{
    Layer *layer = sprite_detach_layer(sprite, index);
    if (layer == NULL) {
        return -1;
    }
    layer_destroy(layer);
    return 0;
}

int sprite_move_layer(Sprite *sprite, int from, int to)
{
    if (from < 0 || from >= sprite->layer_count || to < 0 || to >= sprite->layer_count) {
        return -1;
    }
    Layer *moved = sprite->layers[from];
    for (int i = from; i < to; i++) {
        sprite->layers[i] = sprite->layers[i + 1];
    }
    for (int i = from; i > to; i--) {
        sprite->layers[i] = sprite->layers[i - 1];
    }
    sprite->layers[to] = moved;
    return 0;
}
