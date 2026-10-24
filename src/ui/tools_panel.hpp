#ifndef TINYSPRITE_UI_TOOLS_PANEL_HPP
#define TINYSPRITE_UI_TOOLS_PANEL_HPP

#include "editor.hpp"
#include "imgui.h"
#include "panels.hpp"
#include "theme.hpp"

/* The tool bar (the narrow Tools panel: the tools and the shapes as one column of icon buttons),
 * the options bar above the canvas (the options of the active tool: brush, shape, selection
 * transforms, mirror), and the color block the right panel draws (the color pickers, the palette
 * and the recent colors). */
struct ToolIcon; /* one entry of an icon row, in tools_panel.cpp */

class ToolsPanel
{
  public:
    explicit ToolsPanel(Editor &editor);

    /* Shows another document from now on (a tab was picked); what the panel remembers about the
     * old one is dropped. */
    void setEditor(Editor &editor);

    void show(Panels &panels, float height);
    void colorBlock(const Panels &panels);

    /* The bar above the canvas, drawn as its own window over [x, x + width] x [y, y + height]. */
    void contextBar(float x, float y, float width, float height);

    /* True when the last contextBar had controls hidden past its right end (the bar has no
     * scrollbar; it marks the cut with a chevron). */
    bool barCutOff() const
    {
        return barCutOff_;
    }

    /* True while the "Reset palette" confirmation is open, so the app's shortcuts stay quiet. */
    bool dialogOpen() const
    {
        return dialogOpen_;
    }

    /* Key that selects `tool`, so the app's shortcuts and the button tooltips agree. */
    static ImGuiKey shortcutKey(ToolKind tool);

  private:
    void barCutZone(float width, float height);
    void barCutMark(float width, float height);
    void iconRow(const ToolIcon *first, size_t count);
    void toolGrid();
    void shapeButtons();
    void barSize(const char *tip);
    void barBrush();
    void barFill();
    void barShape(ToolKind tool);
    void barSelection();
    void barSymmetry();
    void colorWidget();
    void recentColors();
    void paletteSwatches();
    void resetDialog();

    Editor *editor_;         /* the document shown; changed by setEditor */
    bool openReset_ = false; /* the confirmation still has to be opened */
    bool dialogOpen_ = false;
    bool barCutOff_ = false;                  /* the options bar has controls past its right end */
    char fgHex_[theme::HEX_BUFFER_SIZE] = ""; /* what is typed in the picker's hex field */
    char bgHex_[theme::HEX_BUFFER_SIZE] = "";
};

#endif
