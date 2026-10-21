#ifndef TINYSPRITE_UI_LAYERS_PANEL_H
#define TINYSPRITE_UI_LAYERS_PANEL_H

#include <cstdint>

#include "editor/editor.h"

/* Layer list (top layer first) with visibility toggles, plus name / opacity of the active layer and
 * add / remove / reorder buttons. Draws into the current ImGui window. */
class LayersPanel
{
  public:
    explicit LayersPanel(Editor &editor);

    void show();

  private:
    void rows();
    void activeLayerDetails();
    void buttons();

    Editor &editor_;

    /* The name and opacity are edited live and one undo step is recorded on release; these hold
     * the values from before that edit began. */
    char nameBefore_[LAYER_NAME_MAX + 1] = "";
    uint8_t opacityBefore_ = 0;
};

#endif
