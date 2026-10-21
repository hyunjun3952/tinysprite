#include "tinysprite/layer_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinysprite/composite.h"

/* Add / remove: the step owns `layer` whenever it is not in the sprite. */
typedef struct Membership {
    Sprite *sprite;
    Layer *layer;
    int index;
    int in_sprite;
} Membership;

static void attach(Membership *m)
{
    if (sprite_insert_layer(m->sprite, m->index, m->layer) == 0) {
        m->in_sprite = 1;
    }
}

static void detach(Membership *m)
{
    if (sprite_detach_layer(m->sprite, m->index) == m->layer) {
        m->in_sprite = 0;
    }
}

static void membership_release(void *ctx)
{
    Membership *m = ctx;
    if (!m->in_sprite) {
        layer_destroy(m->layer);
    }
    free(m);
}

static void add_undo(void *ctx)
{
    detach(ctx);
}

static void add_redo(void *ctx)
{
    attach(ctx);
}

Layer *layer_op_add(History *history, Sprite *sprite, const char *name)
{
    if (!history || !sprite || history_stroke_active(history)) {
        return NULL;
    }
    Membership *m = calloc(1, sizeof(*m));
    Layer *layer = m ? sprite_add_layer(sprite, name) : NULL;
    if (!layer) {
        free(m);
        return NULL;
    }
    *m = (Membership){sprite, layer, sprite_layer_count(sprite) - 1, 1};
    HistoryAction action = {add_undo, add_redo, membership_release, m};
    if (history_push_action(history, action) != 0) {
        /* The release callback has already run; take the layer back out of the sprite. */
        (void)sprite_remove_layer(sprite, sprite_layer_count(sprite) - 1);
        return NULL;
    }
    return layer;
}

int layer_op_remove(History *history, Sprite *sprite, int index)
{
    if (!history || !sprite || history_stroke_active(history) ||
        sprite_layer(sprite, index) == NULL || sprite_layer_count(sprite) < 2) {
        return -1;
    }
    Membership *m = calloc(1, sizeof(*m));
    if (!m) {
        return -1;
    }
    /* Record the step while the layer is still in the sprite so a failed push (which releases the
     * step) cannot destroy it, then detach. */
    *m = (Membership){sprite, sprite_layer(sprite, index), index, 1};
    HistoryAction action = {add_redo, add_undo, membership_release, m};
    if (history_push_action(history, action) != 0) {
        return -1;
    }
    detach(m);
    return 0;
}

Layer *layer_op_duplicate(History *history, Sprite *sprite, int index)
{
    Layer *source = sprite ? sprite_layer(sprite, index) : NULL;
    if (!history || !source || history_stroke_active(history)) {
        return NULL;
    }
    int width = sprite_width(sprite);
    int height = sprite_height(sprite);
    char name[LAYER_NAME_MAX + 1];
    snprintf(name, sizeof(name), "%.*s copy", LAYER_NAME_MAX - 5, layer_name(source));
    Membership *m = calloc(1, sizeof(*m));
    Layer *copy = m ? layer_create(name, width, height, layer_frame_count(source)) : NULL;
    if (!copy) {
        free(m);
        return NULL;
    }
    layer_set_visible(copy, layer_visible(source));
    layer_set_opacity(copy, layer_opacity(source));
    for (int frame = 0; frame < layer_frame_count(source); frame++) {
        memcpy(cel_pixels(layer_cel(copy, frame)), cel_pixels(layer_cel(source, frame)),
               (size_t)width * (size_t)height * sizeof(Rgba));
    }
    if (sprite_insert_layer(sprite, index + 1, copy) != 0) {
        layer_destroy(copy);
        free(m);
        return NULL;
    }
    *m = (Membership){sprite, copy, index + 1, 1};
    HistoryAction action = {add_undo, add_redo, membership_release, m};
    if (history_push_action(history, action) != 0) {
        /* The release callback has already run; take the copy back out of the sprite. */
        (void)sprite_remove_layer(sprite, index + 1);
        return NULL;
    }
    return copy;
}

/* Merge down: the lower layer's cels before and after the merge, per frame, plus the upper layer,
 * which the step owns whenever it is not in the sprite. Restoring whole cels keeps the pixel steps
 * recorded earlier on either layer valid, since undo returns them to exactly the state they saw. */
typedef struct Merge {
    Sprite *sprite;
    Layer *upper, *lower;
    int index; /* the upper layer's index while it is in the sprite */
    int in_sprite;
    int frames;
    size_t pixels;
    Rgba *before; /* frames * pixels */
    Rgba *after;
} Merge;

static void merge_write(const Merge *m, const Rgba *snapshot)
{
    for (int frame = 0; frame < m->frames; frame++) {
        memcpy(cel_pixels(layer_cel(m->lower, frame)), &snapshot[(size_t)frame * m->pixels],
               m->pixels * sizeof(Rgba));
    }
}

static void merge_undo(void *ctx)
{
    Merge *m = ctx;
    merge_write(m, m->before);
    if (sprite_insert_layer(m->sprite, m->index, m->upper) == 0) {
        m->in_sprite = 1;
    }
}

static void merge_redo(void *ctx)
{
    Merge *m = ctx;
    merge_write(m, m->after);
    if (sprite_detach_layer(m->sprite, m->index) == m->upper) {
        m->in_sprite = 0;
    }
}

static void merge_release(void *ctx)
{
    Merge *m = ctx;
    if (!m->in_sprite) {
        layer_destroy(m->upper);
    }
    free(m->before);
    free(m->after);
    free(m);
}

int layer_op_merge_down(History *history, Sprite *sprite, int index)
{
    Layer *upper = sprite ? sprite_layer(sprite, index) : NULL;
    Layer *lower = sprite ? sprite_layer(sprite, index - 1) : NULL;
    if (!history || !upper || !lower || history_stroke_active(history) || !layer_visible(upper) ||
        !layer_visible(lower)) {
        return -1;
    }
    Merge *m = calloc(1, sizeof(*m));
    if (!m) {
        return -1;
    }
    m->sprite = sprite;
    m->upper = upper;
    m->lower = lower;
    m->index = index;
    m->in_sprite = 1;
    m->frames = layer_frame_count(lower);
    m->pixels = (size_t)sprite_width(sprite) * (size_t)sprite_height(sprite);
    m->before = malloc((size_t)m->frames * m->pixels * sizeof(Rgba));
    m->after = malloc((size_t)m->frames * m->pixels * sizeof(Rgba));
    if (!m->before || !m->after) {
        merge_release(m);
        return -1;
    }
    for (int frame = 0; frame < m->frames; frame++) {
        const Rgba *low = cel_pixels(layer_cel(lower, frame));
        const Rgba *up = cel_pixels(layer_cel(upper, frame));
        Rgba *before = &m->before[(size_t)frame * m->pixels];
        Rgba *after = &m->after[(size_t)frame * m->pixels];
        for (size_t i = 0; i < m->pixels; i++) {
            before[i] = low[i];
            after[i] = composite_over(low[i], up[i], layer_opacity(upper));
        }
    }
    /* Record the step while the upper layer is still in the sprite so a failed push (which
     * releases the step) cannot destroy it, then apply. */
    HistoryAction action = {merge_undo, merge_redo, merge_release, m};
    if (history_push_action(history, action) != 0) {
        return -1;
    }
    merge_redo(m);
    return 0;
}

typedef struct Move {
    Sprite *sprite;
    int from, to;
} Move;

static void move_undo(void *ctx)
{
    const Move *m = ctx;
    (void)sprite_move_layer(m->sprite, m->to, m->from);
}

static void move_redo(void *ctx)
{
    const Move *m = ctx;
    (void)sprite_move_layer(m->sprite, m->from, m->to);
}

int layer_op_move(History *history, Sprite *sprite, int from, int to)
{
    if (!history || !sprite || history_stroke_active(history) ||
        sprite_layer(sprite, from) == NULL || sprite_layer(sprite, to) == NULL) {
        return -1;
    }
    if (from == to) {
        return 0;
    }
    Move *m = malloc(sizeof(*m));
    if (!m) {
        return -1;
    }
    *m = (Move){sprite, from, to};
    HistoryAction action = {move_undo, move_redo, free, m};
    if (history_push_action(history, action) != 0) {
        return -1;
    }
    move_redo(m);
    return 0;
}

typedef enum Property { PROP_NAME, PROP_VISIBLE, PROP_OPACITY } Property;

typedef struct PropertyChange {
    Layer *layer;
    Property property;
    char old_name[LAYER_NAME_MAX + 1];
    char new_name[LAYER_NAME_MAX + 1];
    int old_value, new_value; /* visibility / opacity */
} PropertyChange;

static void property_set(const PropertyChange *c, const char *name, int value)
{
    switch (c->property) {
    case PROP_NAME:
        layer_set_name(c->layer, name);
        break;
    case PROP_VISIBLE:
        layer_set_visible(c->layer, value);
        break;
    case PROP_OPACITY:
        layer_set_opacity(c->layer, (uint8_t)value);
        break;
    }
}

static void property_undo(void *ctx)
{
    const PropertyChange *c = ctx;
    property_set(c, c->old_name, c->old_value);
}

static void property_redo(void *ctx)
{
    const PropertyChange *c = ctx;
    property_set(c, c->new_name, c->new_value);
}

static int record_property(History *history, PropertyChange *change)
{
    HistoryAction action = {property_undo, property_redo, free, change};
    if (history_push_action(history, action) != 0) {
        return -1;
    }
    property_redo(change);
    return 0;
}

static PropertyChange *property_change(History *history, Layer *layer, Property property)
{
    if (!history || !layer || history_stroke_active(history)) {
        return NULL;
    }
    PropertyChange *change = calloc(1, sizeof(*change));
    if (change) {
        change->layer = layer;
        change->property = property;
    }
    return change;
}

int layer_op_set_name(History *history, Layer *layer, const char *name)
{
    PropertyChange *change = property_change(history, layer, PROP_NAME);
    if (!change) {
        return -1;
    }
    /* Round-trip through a layer so truncation matches layer_set_name. */
    snprintf(change->old_name, sizeof(change->old_name), "%s", layer_name(layer));
    snprintf(change->new_name, sizeof(change->new_name), "%.*s", LAYER_NAME_MAX, name ? name : "");
    if (strcmp(change->old_name, change->new_name) == 0) {
        free(change);
        return 0;
    }
    return record_property(history, change);
}

int layer_op_set_visible(History *history, Layer *layer, int visible)
{
    PropertyChange *change = property_change(history, layer, PROP_VISIBLE);
    if (!change) {
        return -1;
    }
    change->old_value = layer_visible(layer) != 0;
    change->new_value = visible != 0;
    if (change->old_value == change->new_value) {
        free(change);
        return 0;
    }
    return record_property(history, change);
}

int layer_op_set_opacity(History *history, Layer *layer, uint8_t opacity)
{
    PropertyChange *change = property_change(history, layer, PROP_OPACITY);
    if (!change) {
        return -1;
    }
    change->old_value = layer_opacity(layer);
    change->new_value = opacity;
    if (change->old_value == change->new_value) {
        free(change);
        return 0;
    }
    return record_property(history, change);
}
