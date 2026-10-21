#ifndef TINYSPRITE_UI_TOOLS_H
#define TINYSPRITE_UI_TOOLS_H

#include <memory>

#include "editor/drag.h"
#include "editor/editor.h"

/* Applies the editor's active tool to a press at cel (cx, cy). Returns the drag that follows the
 * mouse until the release, or null if the tool needs none (the bucket's single click) or does not
 * accept the press (a hidden layer). Used by Editor::press. */
std::unique_ptr<Drag> beginDrag(Editor &editor, int cx, int cy);

#endif
