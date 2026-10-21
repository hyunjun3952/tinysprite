#include "tinysprite/layer_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinysprite/composite.h"

/* Ownership rule for every op below: a layer belongs to the sprite while it is attached and to
 * the step that detached it otherwise, so the history frees it only when it is dropped detached.
 * history_push_action releases the step when it fails, which is why an op either records its step
 * while the layer is still attached (remove, merge) or takes the layer back out of the sprite
 * after a failed push (add, duplicate). */

static int record(History *history, HistoryAction action, bool apply_now)
{
    if (history_push_action(history, action) != 0) {
        return -1;
    }
    if (apply_now) {
        action.redo(action.ctx);
    }
    return 0;
}

static bool editable(const History *history, const Layer *layer)
{
    return history && layer && !history_stroke_active(history);
}

/* Add / remove / merge: where a layer sits in its sprite, and whether it does right now. */
typedef struct Membership {
    Sprite *sprite;
    Layer *layer;
    int index;
    bool in_sprite;
} Membership;

static void membership_attach(Membership *m)
{
    if (sprite_insert_layer(m->sprite, m->index, m->layer) == 0) {
        m->in_sprite = true;
    }
}

static void membership_detach(Membership *m)
{
    if (sprite_detach_layer(m->sprite, m->index) == m->layer) {
        m->in_sprite = false;
    }
}

static void membership_destroy_if_detached(Membership *m)
{
    if (!m->in_sprite) {
        layer_destroy(m->layer);
    }
}

static void attach_step(void *ctx)
{
    membership_attach(ctx);
}

static void detach_step(void *ctx)
{
    membership_detach(ctx);
}

static void membership_release(void *ctx)
{
    Membership *m = ctx;
    membership_destroy_if_detached(m);
    free(m);
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
    *m = (Membership){sprite, layer, sprite_layer_count(sprite) - 1, true};
    if (record(history, (HistoryAction){detach_step, attach_step, membership_release, m}, false) !=
        0) {
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
    Membership *m = malloc(sizeof(*m));
    if (!m) {
        return -1;
    }
    *m = (Membership){sprite, sprite_layer(sprite, index), index, true};
    return record(history, (HistoryAction){attach_step, detach_step, membership_release, m}, true);
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
    *m = (Membership){sprite, copy, index + 1, true};
    if (record(history, (HistoryAction){detach_step, attach_step, membership_release, m}, false) !=
        0) {
        (void)sprite_remove_layer(sprite, index + 1);
        return NULL;
    }
    return copy;
}

/* Merge down: the lower layer's cels before and after the merge, per frame, plus the upper layer.
 * Restoring whole cels keeps the pixel steps recorded earlier on either layer valid, since undo
 * returns them to exactly the state they saw. */
typedef struct Merge {
    Membership upper;
    Layer *lower;
    int frames;
    size_t pixels; /* per frame */
    Rgba *before;  /* frames * pixels */
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
    membership_attach(&m->upper);
}

static void merge_redo(void *ctx)
{
    Merge *m = ctx;
    merge_write(m, m->after);
    membership_detach(&m->upper);
}

static void merge_release(void *ctx)
{
    Merge *m = ctx;
    membership_destroy_if_detached(&m->upper);
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
    m->upper = (Membership){sprite, upper, index, true};
    m->lower = lower;
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
    return record(history, (HistoryAction){merge_undo, merge_redo, merge_release, m}, true);
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
    return record(history, (HistoryAction){move_undo, move_redo, free, m}, true);
}

typedef struct NameChange {
    Layer *layer;
    char old_name[LAYER_NAME_MAX + 1];
    char new_name[LAYER_NAME_MAX + 1];
} NameChange;

static void name_undo(void *ctx)
{
    const NameChange *c = ctx;
    layer_set_name(c->layer, c->old_name);
}

static void name_redo(void *ctx)
{
    const NameChange *c = ctx;
    layer_set_name(c->layer, c->new_name);
}

/* Records old -> new. `apply_now` is false when the layer already holds `new_name`. */
static int change_name(History *history, Layer *layer, const char *old_name, const char *new_name,
                       bool apply_now)
{
    if (!editable(history, layer)) {
        return -1;
    }
    NameChange *c = calloc(1, sizeof(*c));
    if (!c) {
        return -1;
    }
    c->layer = layer;
    /* Same truncation as layer_set_name, so the comparison and the undo match what the layer holds.
     */
    snprintf(c->old_name, sizeof(c->old_name), "%.*s", LAYER_NAME_MAX, old_name ? old_name : "");
    snprintf(c->new_name, sizeof(c->new_name), "%.*s", LAYER_NAME_MAX, new_name ? new_name : "");
    if (strcmp(c->old_name, c->new_name) == 0) {
        free(c);
        return 0;
    }
    return record(history, (HistoryAction){name_undo, name_redo, free, c}, apply_now);
}

int layer_op_set_name(History *history, Layer *layer, const char *name)
{
    return change_name(history, layer, layer ? layer_name(layer) : NULL, name, true);
}

int layer_op_record_name(History *history, Layer *layer, const char *before)
{
    return change_name(history, layer, before, layer ? layer_name(layer) : NULL, false);
}

/* Visibility and opacity are both a small integer behind a setter. */
typedef struct ValueChange {
    Layer *layer;
    void (*set)(Layer *layer, int value);
    int old_value, new_value;
} ValueChange;

static void value_undo(void *ctx)
{
    const ValueChange *c = ctx;
    c->set(c->layer, c->old_value);
}

static void value_redo(void *ctx)
{
    const ValueChange *c = ctx;
    c->set(c->layer, c->new_value);
}

static void set_visible_value(Layer *layer, int value)
{
    layer_set_visible(layer, value != 0);
}

static void set_opacity_value(Layer *layer, int value)
{
    layer_set_opacity(layer, (uint8_t)value);
}

static int change_value(History *history, Layer *layer, void (*set)(Layer *, int), int old_value,
                        int new_value, bool apply_now)
{
    if (!editable(history, layer)) {
        return -1;
    }
    if (old_value == new_value) {
        return 0;
    }
    ValueChange *c = malloc(sizeof(*c));
    if (!c) {
        return -1;
    }
    *c = (ValueChange){layer, set, old_value, new_value};
    return record(history, (HistoryAction){value_undo, value_redo, free, c}, apply_now);
}

int layer_op_set_visible(History *history, Layer *layer, bool visible)
{
    int old_value = layer && layer_visible(layer);
    return change_value(history, layer, set_visible_value, old_value, (int)visible, true);
}

int layer_op_set_opacity(History *history, Layer *layer, uint8_t opacity)
{
    return change_value(history, layer, set_opacity_value, layer ? layer_opacity(layer) : 0,
                        opacity, true);
}

int layer_op_record_opacity(History *history, Layer *layer, uint8_t before)
{
    return change_value(history, layer, set_opacity_value, before, layer ? layer_opacity(layer) : 0,
                        false);
}
