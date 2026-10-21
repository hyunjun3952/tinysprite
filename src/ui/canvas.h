#ifndef TINYSPRITE_UI_CANVAS_H
#define TINYSPRITE_UI_CANVAS_H

#include "editor.h"

/* The canvas window: draws the checkerboard, the composited sprite and the pixel grid, and turns
 * mouse input into zoom, pan and pencil/eraser strokes. Call once per frame between
 * imgui_backend_new_frame and imgui_backend_render. `x`, `y`, `w`, `h` place the window. */
void canvas_show(Editor *editor, float x, float y, float w, float h);

#endif
