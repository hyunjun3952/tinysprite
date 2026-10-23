#ifndef TINYSPRITE_UI_TILEMAP_PANEL_HPP
#define TINYSPRITE_UI_TILEMAP_PANEL_HPP

#include "editor.hpp"

/* The Tilemaps section of the Inspector: the tilesets cut from the cel and the tilemaps made from
 * them as two lists with their buttons, the name, visibility and size of the selected map, and the
 * button that opens the tilemap window where the tiles are painted. Every change is one undo step
 * (the Editor tilemap functions). */
class TilemapPanel
{
  public:
    explicit TilemapPanel(Editor &editor);

    /* Shows another document from now on. */
    void setEditor(Editor &editor);

    /* The content of the section; call it inside the Inspector window. */
    void show();

  private:
    void tilesets();
    void maps();
    void details(int index);

    Editor *editor_; /* the document shown; changed by setEditor */
    int tileWidth_ = 16;
    int tileHeight_ = 16;
    int newColumns_ = 16;
    int newRows_ = 16;
    int columns_ = 16; /* the size fields of the selected map, reloaded when another is selected */
    int rows_ = 16;
    int shownMap_ = -1;
    uint64_t shownRevision_ = 0;
    char name_[TILE_NAME_MAX + 1] = "";
    bool nameActive_ = false;
};

#endif
