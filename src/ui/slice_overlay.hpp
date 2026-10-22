#ifndef TINYSPRITE_UI_SLICE_OVERLAY_HPP
#define TINYSPRITE_UI_SLICE_OVERLAY_HPP

#include "editor.hpp"
#include "imgui.h"

/* Draws the sprite's slices over the canvas: an outline and the name for each (the selected one in
 * another color and lightly filled), a cross at a pivot, and the rectangle being dragged out.
 * `origin` is the top-left of the canvas window; the editor's viewport maps cels to it. */
void draw_slices(ImDrawList *draw, ImVec2 origin, const Editor &editor);

#endif
