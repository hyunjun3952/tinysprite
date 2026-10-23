#ifndef TINYSPRITE_UI_TILEMAP_WINDOW_HPP
#define TINYSPRITE_UI_TILEMAP_WINDOW_HPP

#include <SDL.h>

#include <cstdint>

#include "cel_texture.hpp"
#include "editor.hpp"

/* The floating window where the selected tilemap is edited: the tiles of its tileset to pick from
 * (and an eraser), the map drawn at a zoom of its own, and painting with the mouse (left button
 * the picked tile, right button erases; one drag is one undo step). */
class TilemapWindow
{
  public:
    /* Draws the window if `editor.overlays().tilemap_window` is on; closing it turns that off. */
    void show(SDL_Renderer *renderer, Editor &editor);

    /* Frees the textures; call before the renderer goes. */
    void release();

  private:
    void refresh(SDL_Renderer *renderer, const Editor &editor, const Tileset &tileset,
                 const TileMap &map);
    void toolbar(Editor &editor, const TileMap &map);
    void picker(Editor &editor, const Tileset &tileset);
    void mapView(Editor &editor, const Tileset &tileset, const TileMap &map);
    void paint(Editor &editor, const Tileset &tileset, const TileMap &map, float x0, float y0);

    CelTexture atlas_;
    CelTexture picture_;
    const Editor *shownEditor_ = nullptr;
    uint64_t shownRevision_ = 0;
    int shownMap_ = -1;
    const Cel *shownAtlas_ = nullptr;
    int zoom_ = 3;
    int lastX_ = -1; /* the cell the last paint step reached, for joining the steps with a line */
    int lastY_ = -1;
};

#endif
