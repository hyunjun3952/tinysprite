#ifndef TINYSPRITE_UI_TOOLS_PANEL_H
#define TINYSPRITE_UI_TOOLS_PANEL_H

#include "editor/editor.h"
#include "imgui.h"
#include "panels/palette_panel.h"
#include "panels/panels.h"

/* The Tools window: tool buttons, tool options, selection transforms, the color pickers and the
 * palette. */
class ToolsPanel
{
  public:
    explicit ToolsPanel(Editor &editor);

    void show(Panels &panels, float height);

    /* Key that selects `tool`, so the app's shortcuts and the button tooltips agree. */
    static ImGuiKey shortcutKey(ToolKind tool);

  private:
    void colorWidget();
    void selectionButtons();

    Editor &editor_;
    PalettePanel palette_;
};

#endif
