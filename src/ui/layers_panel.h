#ifndef TINYSPRITE_UI_LAYERS_PANEL_H
#define TINYSPRITE_UI_LAYERS_PANEL_H

#include "editor.h"

/* Layer list (top layer first) with visibility toggles, plus name / opacity of the active layer and
 * add / remove / reorder buttons. Draws into the current ImGui window. */
void layers_panel_show(Editor *editor);

#endif
