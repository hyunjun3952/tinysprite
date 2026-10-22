#ifndef TINYSPRITE_UI_TOOLS_PANEL_HPP
#define TINYSPRITE_UI_TOOLS_PANEL_HPP

#include "editor.hpp"
#include "imgui.h"
#include "panels.hpp"

/* The Tools window, in collapsible sections: the tools, the options of the active tool, symmetry,
 * selection transforms, the color pickers with the palette, and the recent colors. */
struct ToolIcon; /* one entry of an icon row, in tools_panel.cpp */

class ToolsPanel
{
  public:
    explicit ToolsPanel(Editor &editor);

    void show(Panels &panels, float height);

    /* True while the "Reset palette" confirmation is open, so the app's shortcuts stay quiet. */
    bool dialogOpen() const
    {
        return dialogOpen_;
    }

    /* Key that selects `tool`, so the app's shortcuts and the button tooltips agree. */
    static ImGuiKey shortcutKey(ToolKind tool);

  private:
    void iconRow(const ToolIcon *first, size_t count);
    void toolGrid();
    void shapeButtons();
    void toolOptions();
    void symmetryOptions();
    void colorWidget();
    void selectionButtons();
    void recentColors();
    void paletteSwatches();
    void resetDialog();

    Editor &editor_;
    bool openReset_ = false; /* the confirmation still has to be opened */
    bool dialogOpen_ = false;
};

#endif
