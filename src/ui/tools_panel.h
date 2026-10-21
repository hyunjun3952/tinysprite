#ifndef TINYSPRITE_UI_TOOLS_PANEL_H
#define TINYSPRITE_UI_TOOLS_PANEL_H

#include "editor.h"
#include "imgui.h"
#include "panels.h"

/* The Tools window, in collapsible sections: the tools, the options of the active tool, symmetry,
 * selection transforms, the color pickers, the recent colors and the palette. */
class ToolsPanel
{
  public:
    explicit ToolsPanel(Editor &editor);

    void show(Panels &panels, float height);

    /* Key that selects `tool`, so the app's shortcuts and the button tooltips agree. */
    static ImGuiKey shortcutKey(ToolKind tool);

  private:
    void toolGrid();
    void toolOptions();
    void symmetryOptions();
    void colorWidget();
    void selectionButtons();
    void recentColors();
    void paletteSwatches();

    Editor &editor_;
};

#endif
