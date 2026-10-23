#ifndef TINYSPRITE_TILEMAP_OPS_HPP
#define TINYSPRITE_TILEMAP_OPS_HPP

#include <memory>
#include <vector>

#include "tinysprite/history.hpp"
#include "tinysprite/sprite.hpp"

/* Undoable edits of the tilesets and tilemaps of a sprite, in the style of slice_ops.hpp: each call
 * applies the change and records it as one undo step, and fails without changing anything for an
 * invalid value or index, or while a stroke is active. Setting a map to what it already is returns
 * true and records nothing. */

bool tileset_op_add(History &history, Sprite &sprite, Tileset tileset);

/* Refused while a tilemap uses the tileset; the maps of later tilesets follow their tileset to its
 * new index. */
bool tileset_op_remove(History &history, Sprite &sprite, int index);

bool tilemap_op_add(History &history, Sprite &sprite, TileMap map);
bool tilemap_op_remove(History &history, Sprite &sprite, int index);

/* Replaces the map at `index` (rename, show / hide, another tileset or size). Names longer than
 * TILE_NAME_MAX bytes are cut. */
bool tilemap_op_set(History &history, Sprite &sprite, int index, TileMap map);

/* Gives the map another grid size 1..TILEMAP_MAX_DIMENSION, keeping the cells that stay inside
 * and leaving new ones empty. */
bool tilemap_op_resize(History &history, Sprite &sprite, int index, int columns, int rows);

/* Sets every cell of `area` (clipped to the map) to `cell` (0 erases) as one undo step. */
bool tilemap_op_fill(History &history, Sprite &sprite, int map, Rect area, uint32_t cell);

/* Cells painted with the mouse. Every set() shows at once; end() records all of them as one undo
 * step (nothing when no cell changed), cancel() or destroying the stroke puts them back. The
 * sprite and history must outlive it. */
class TileStroke
{
  public:
    /* Null for a map out of range or while the history has a stroke open. */
    static std::unique_ptr<TileStroke> begin(History &history, Sprite &sprite, int map);

    TileStroke(const TileStroke &) = delete;
    TileStroke &operator=(const TileStroke &) = delete;
    ~TileStroke();

    /* Returns false for a cell outside the map or a tile the tileset does not have. */
    bool set(int x, int y, uint32_t cell);

    bool end();
    void cancel();

  private:
    TileStroke(History &history, Sprite &sprite, int map);

    struct Change {
        int x, y;
        uint32_t before, after;
    };

    History &history_;
    Sprite &sprite_;
    int map_;
    bool open_ = true;
    std::vector<Change> changes_;
};

#endif
