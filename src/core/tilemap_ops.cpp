#include "tinysprite/tilemap_ops.hpp"

#include <algorithm>
#include <utility>

namespace
{

/* Records the change of all tile data from what it is now to the given lists and applies it. */
bool replaceTileData(History &history, Sprite &sprite, std::vector<Tileset> tilesets,
                     std::vector<TileMap> maps)
{
    if (!tile_data_valid(tilesets, maps)) {
        return false;
    }
    std::vector<Tileset> oldSets = sprite.tilesets();
    std::vector<TileMap> oldMaps = sprite.tilemaps();
    size_t cells = 0;
    for (const TileMap &map : oldMaps) {
        cells += map.cells.size();
    }
    for (const TileMap &map : maps) {
        cells += map.cells.size();
    }
    HistoryAction action;
    action.bytes = cells * sizeof(uint32_t);
    action.undo = [&sprite, oldSets, oldMaps] { sprite.setTileData(oldSets, oldMaps); };
    action.redo = [&sprite, tilesets = std::move(tilesets), maps = std::move(maps)] {
        sprite.setTileData(tilesets, maps);
    };
    return history.perform(std::move(action));
}

bool mapIndexValid(const Sprite &sprite, int index)
{
    return index >= 0 && index < static_cast<int>(sprite.tilemaps().size());
}

} // namespace

bool tileset_op_add(History &history, Sprite &sprite, Tileset tileset)
{
    if (history.strokeActive() || !tileset_valid(tileset)) {
        return false;
    }
    tileset.name = layer_name_truncate(tileset.name, TILE_NAME_MAX);
    std::vector<Tileset> next = sprite.tilesets();
    next.push_back(std::move(tileset));
    return replaceTileData(history, sprite, std::move(next), sprite.tilemaps());
}

bool tileset_op_remove(History &history, Sprite &sprite, int index)
{
    if (history.strokeActive() || index < 0 ||
        index >= static_cast<int>(sprite.tilesets().size())) {
        return false;
    }
    std::vector<TileMap> maps = sprite.tilemaps();
    for (TileMap &map : maps) {
        if (map.tileset == index) {
            return false;
        }
        if (map.tileset > index) {
            map.tileset--;
        }
    }
    std::vector<Tileset> next = sprite.tilesets();
    next.erase(next.begin() + index);
    return replaceTileData(history, sprite, std::move(next), std::move(maps));
}

bool tilemap_op_add(History &history, Sprite &sprite, TileMap map)
{
    if (history.strokeActive()) {
        return false;
    }
    map.name = layer_name_truncate(map.name, TILE_NAME_MAX);
    std::vector<TileMap> next = sprite.tilemaps();
    next.push_back(std::move(map));
    return replaceTileData(history, sprite, sprite.tilesets(), std::move(next));
}

bool tilemap_op_remove(History &history, Sprite &sprite, int index)
{
    if (history.strokeActive() || !mapIndexValid(sprite, index)) {
        return false;
    }
    std::vector<TileMap> next = sprite.tilemaps();
    next.erase(next.begin() + index);
    return replaceTileData(history, sprite, sprite.tilesets(), std::move(next));
}

bool tilemap_op_set(History &history, Sprite &sprite, int index, TileMap map)
{
    if (history.strokeActive() || !mapIndexValid(sprite, index)) {
        return false;
    }
    map.name = layer_name_truncate(map.name, TILE_NAME_MAX);
    const TileMap &now = sprite.tilemaps()[static_cast<size_t>(index)];
    if (now.name == map.name && now.tileset == map.tileset && now.columns == map.columns &&
        now.rows == map.rows && now.visible == map.visible && now.cells == map.cells) {
        return true;
    }
    std::vector<TileMap> next = sprite.tilemaps();
    next[static_cast<size_t>(index)] = std::move(map);
    return replaceTileData(history, sprite, sprite.tilesets(), std::move(next));
}

bool tilemap_op_resize(History &history, Sprite &sprite, int index, int columns, int rows)
{
    if (history.strokeActive() || !mapIndexValid(sprite, index) || columns < 1 ||
        columns > TILEMAP_MAX_DIMENSION || rows < 1 || rows > TILEMAP_MAX_DIMENSION) {
        return false;
    }
    const TileMap &now = sprite.tilemaps()[static_cast<size_t>(index)];
    TileMap next = tilemap_blank(now.name, now.tileset, columns, rows);
    next.visible = now.visible;
    for (int y = 0; y < std::min(rows, now.rows); y++) {
        for (int x = 0; x < std::min(columns, now.columns); x++) {
            next.cells[static_cast<size_t>(y) * static_cast<size_t>(columns) +
                       static_cast<size_t>(x)] = now.cell(x, y);
        }
    }
    return tilemap_op_set(history, sprite, index, std::move(next));
}

bool tilemap_op_fill(History &history, Sprite &sprite, int map, Rect area, uint32_t cell)
{
    std::unique_ptr<TileStroke> stroke = TileStroke::begin(history, sprite, map);
    if (!stroke) {
        return false;
    }
    const TileMap &target = sprite.tilemaps()[static_cast<size_t>(map)];
    Rect r = rect_clip(area, target.columns, target.rows);
    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            if (!stroke->set(x, y, cell)) {
                return false; /* a tile the tileset lacks: the stroke undoes itself */
            }
        }
    }
    return stroke->end();
}

std::unique_ptr<TileStroke> TileStroke::begin(History &history, Sprite &sprite, int map)
{
    if (history.strokeActive() || !mapIndexValid(sprite, map)) {
        return nullptr;
    }
    return std::unique_ptr<TileStroke>(new TileStroke(history, sprite, map));
}

TileStroke::TileStroke(History &history, Sprite &sprite, int map)
    : history_(history), sprite_(sprite), map_(map)
{
}

TileStroke::~TileStroke()
{
    cancel();
}

bool TileStroke::set(int x, int y, uint32_t cell)
{
    if (!open_) {
        return false;
    }
    uint32_t before = sprite_.tilemaps()[static_cast<size_t>(map_)].cell(x, y);
    if (!sprite_.setTile(map_, x, y, cell)) {
        return false;
    }
    if (before != cell) {
        changes_.push_back(Change{x, y, before, cell});
    }
    return true;
}

void TileStroke::cancel()
{
    if (!open_) {
        return;
    }
    open_ = false;
    for (auto it = changes_.rbegin(); it != changes_.rend(); ++it) {
        sprite_.setTile(map_, it->x, it->y, it->before);
    }
    changes_.clear();
}

bool TileStroke::end()
{
    if (!open_ || history_.strokeActive()) {
        return false;
    }
    open_ = false;
    if (changes_.empty()) {
        return true;
    }
    HistoryAction action;
    action.bytes = changes_.size() * sizeof(Change);
    Sprite &sprite = sprite_;
    int map = map_;
    action.undo = [&sprite, map, changes = changes_] {
        for (auto it = changes.rbegin(); it != changes.rend(); ++it) {
            sprite.setTile(map, it->x, it->y, it->before);
        }
    };
    action.redo = [&sprite, map, changes = std::move(changes_)] {
        for (const Change &c : changes) {
            sprite.setTile(map, c.x, c.y, c.after);
        }
    };
    return history_.pushAction(std::move(action));
}
