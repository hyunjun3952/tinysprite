#ifndef TINYSPRITE_LAYER_OPS_HPP
#define TINYSPRITE_LAYER_OPS_HPP

#include <cstdint>
#include <string>

#include "tinysprite/history.hpp"
#include "tinysprite/sprite.hpp"

/* Undoable layer edits: each call applies the change and records it as one undo step. All of them
 * fail without changing anything while a stroke is active. A layer removed (or added and then
 * undone) stays alive inside the history, so its cels and the pixel steps recorded on them remain
 * valid; the history frees it when the step is dropped. A property op that would not change the
 * value returns true and records nothing. */

/* Appends a layer on top. Returns it, or null on a stroke in progress. */
Layer *layer_op_add(History &history, Sprite &sprite, const std::string &name);

/* Returns false if index is out of range, only one layer is left, or a stroke is active. */
bool layer_op_remove(History &history, Sprite &sprite, int index);

/* Moves the layer at `from` to index `to`. Returns true on success (including from == to), false
 * if either index is out of range or a stroke is active. */
bool layer_op_move(History &history, Sprite &sprite, int from, int to);

/* Inserts a copy of the layer at `index` (name + " copy", same visibility, opacity, blend mode and
 * pixels in every frame) just above it. Returns the copy, or null if index is out of range or a
 * stroke is active. */
Layer *layer_op_duplicate(History &history, Sprite &sprite, int index);

/* Whether `upper` can be drawn onto `lower`: both are visible and `lower` is not locked (`upper` is
 * only read). The rule behind layer_op_merge_down, for callers that ask first. */
bool layer_can_merge(const Layer &upper, const Layer &lower);

/* Whether merging `upper` into `lower` leaves the flattened image as it was. Blend modes see
 * everything under a layer, a merge only sees `lower`, so it is exact when both are Normal (plain
 * "over" nests) or when no visible layer lies under `lower` (`visibleBelow` false). It never is
 * when `lower` is not fully opaque: it keeps that opacity, so the merged pixels would fade with it.
 */
bool layer_merge_keeps_look(const Layer &upper, const Layer &lower, bool visibleBelow);

/* Whether any layer below `index` (indexes 0..index-1) is visible: the `visibleBelow` argument of
 * layer_merge_keeps_look for a layer sitting at `index`. */
bool layer_any_visible_below(const Sprite &sprite, int index);

/* Whether layer_op_merge_down(index) would go through: a layer below, layer_can_merge and
 * layer_merge_keeps_look. For callers that ask first. */
bool layer_can_merge_down(const Sprite &sprite, int index);

/* Draws the layer at `index` onto the layer below it (every frame, with the upper layer's blend
 * mode and opacity applied to its pixels) and removes it; the lower layer keeps its own name and
 * properties. The pixel change and the removal are one undo step. Returns false if there is no
 * layer below, either layer is hidden, the lower layer is locked, the merge would change how the
 * stack looks (see layer_merge_keeps_look), or a stroke is active. */
bool layer_op_merge_down(History &history, Sprite &sprite, int index);

bool layer_op_set_name(History &history, Layer &layer, const std::string &name);
bool layer_op_set_visible(History &history, Layer &layer, bool visible);
bool layer_op_set_locked(History &history, Layer &layer, bool locked);
bool layer_op_set_opacity(History &history, Layer &layer, uint8_t opacity);
bool layer_op_set_blend_mode(History &history, Layer &layer, BlendMode mode);

/* For an edit made live with Layer::set*: the layer already holds the new value and `before` is
 * what it held when the edit began. Records that change as one undo step without applying it
 * again (nothing if the value did not change). Typing a name or dragging a slider calls this once
 * when the widget is released. Returns false on an active stroke. */
bool layer_op_record_name(History &history, Layer &layer, const std::string &before);
bool layer_op_record_opacity(History &history, Layer &layer, uint8_t before);

#endif
