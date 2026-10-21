#include "tinysprite/layer.h"

#include <stdio.h>
#include <stdlib.h>

struct Layer {
    char name[LAYER_NAME_MAX + 1];
    int visible;
    uint8_t opacity;
    int frame_count;
    Cel **cels;
};

Layer *layer_create(const char *name, int width, int height, int frame_count)
{
    if (frame_count < 1) {
        return NULL;
    }
    Layer *layer = calloc(1, sizeof(Layer));
    if (layer == NULL) {
        return NULL;
    }
    layer->cels = (Cel **)calloc((size_t)frame_count, sizeof(Cel *));
    if (layer->cels == NULL) {
        free(layer);
        return NULL;
    }
    layer->visible = 1;
    layer->opacity = 255;
    layer->frame_count = frame_count;

    if (name != NULL) {
        /* Truncates to LAYER_NAME_MAX and always NUL-terminates. */
        snprintf(layer->name, sizeof(layer->name), "%.*s", LAYER_NAME_MAX, name);
    }

    for (int i = 0; i < frame_count; i++) {
        layer->cels[i] = cel_create(width, height);
        if (layer->cels[i] == NULL) {
            layer_destroy(layer);
            return NULL;
        }
    }
    return layer;
}

void layer_destroy(Layer *layer)
{
    if (layer == NULL) {
        return;
    }
    for (int i = 0; i < layer->frame_count; i++) {
        cel_destroy(layer->cels[i]);
    }
    free((void *)layer->cels);
    free(layer);
}

const char *layer_name(const Layer *layer)
{
    return layer->name;
}

int layer_visible(const Layer *layer)
{
    return layer->visible;
}

void layer_set_visible(Layer *layer, int visible)
{
    layer->visible = visible != 0;
}

uint8_t layer_opacity(const Layer *layer)
{
    return layer->opacity;
}

void layer_set_opacity(Layer *layer, uint8_t opacity)
{
    layer->opacity = opacity;
}

int layer_frame_count(const Layer *layer)
{
    return layer->frame_count;
}

Cel *layer_cel(Layer *layer, int frame)
{
    if (frame < 0 || frame >= layer->frame_count) {
        return NULL;
    }
    return layer->cels[frame];
}
