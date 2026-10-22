#ifndef TINYSPRITE_CEL_OPS_HPP
#define TINYSPRITE_CEL_OPS_HPP

#include "tinysprite/history.hpp"
#include "tinysprite/layer.hpp"

/* Undoable edits of how a layer's frames share cels. Each call applies the change and records it
 * as one undo step; all fail without changing anything while a stroke is active or for a frame out
 * of range. A cel that stops being shown stays alive inside the history, so the pixel steps
 * recorded on it remain valid. */

/* Makes `to` show the same cel as `from` (a linked cel), dropping what `to` held. Linking frames
 * that already share a cel returns true and records nothing. */
bool cel_op_link(History &history, Layer &layer, int from, int to);

/* Gives `frame` its own copy of the cel it shares. An unlinked cel returns true and records
 * nothing. */
bool cel_op_unlink(History &history, Layer &layer, int frame);

#endif
