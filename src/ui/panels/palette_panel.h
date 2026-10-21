#ifndef TINYSPRITE_UI_PALETTE_PANEL_H
#define TINYSPRITE_UI_PALETTE_PANEL_H

#include "editor/editor.h"

/* The recent-colors row and the sprite palette swatches, inside the tools window. Clicking a
 * swatch makes it the foreground color. */
class PalettePanel
{
  public:
    explicit PalettePanel(Editor &editor);

    void show();

  private:
    void recentRow();
    void paletteSwatches();

    Editor &editor_;
};

#endif
