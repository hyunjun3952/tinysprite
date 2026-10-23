/* Editor: tilesets and tilemaps. */

#include <algorithm>
#include <string>
#include <utility>

#include "editor.hpp"

int Editor::selectedTilemap() const
{
    return selectedTilemap_ < static_cast<int>(sprite_.tilemaps().size()) ? selectedTilemap_ : -1;
}

int Editor::selectedTileset() const
{
    return selectedTileset_ < static_cast<int>(sprite_.tilesets().size()) ? selectedTileset_ : -1;
}

void Editor::selectTilemap(int index)
{
    selectedTilemap_ =
        index >= 0 && index < static_cast<int>(sprite_.tilemaps().size()) ? index : -1;
}

void Editor::selectTileset(int index)
{
    selectedTileset_ =
        index >= 0 && index < static_cast<int>(sprite_.tilesets().size()) ? index : -1;
}

uint32_t Editor::selectedTile() const
{
    int map = selectedTilemap();
    if (map < 0) {
        return selectedTile_;
    }
    const Tileset &tileset =
        sprite_
            .tilesets()[static_cast<size_t>(sprite_.tilemaps()[static_cast<size_t>(map)].tileset)];
    return std::min(selectedTile_, static_cast<uint32_t>(tileset.tileCount()));
}

void Editor::selectTile(uint32_t cell)
{
    selectedTile_ = cell;
}

namespace
{

/* "<prefix> N" for the smallest N that none of the names has. */
template <class Items, class NameOf>
std::string freshName(const std::string &prefix, const Items &items, NameOf nameOf)
{
    for (int n = 1;; n++) {
        const std::string name = prefix + " " + std::to_string(n);
        bool taken = std::any_of(items.begin(), items.end(),
                                 [&](const auto &item) { return nameOf(item) == name; });
        if (!taken) {
            return name;
        }
    }
}

} // namespace

bool Editor::addTilesetFromCel(int tileWidth, int tileHeight)
{
    Cel *cel = activeCel();
    if (stroking() || !cel) {
        return false;
    }
    Rect area = selection_.value_or(Rect{0, 0, cel->width(), cel->height()});
    std::string name = freshName("Tileset", sprite_.tilesets(),
                                 [](const Tileset &t) -> const std::string & { return t.name; });
    std::optional<Tileset> tileset = tileset_from_cel(*cel, area, tileWidth, tileHeight, name);
    bool done = false;
    if (tileset) {
        guarded([&] { done = tileset_op_add(history_, sprite_, std::move(*tileset)); });
    }
    if (done) {
        selectedTileset_ = static_cast<int>(sprite_.tilesets().size()) - 1;
        tilesRevision_++;
    }
    return done;
}

bool Editor::removeTileset(int index)
{
    if (stroking() || !tileset_op_remove(history_, sprite_, index)) {
        return false;
    }
    selectedTileset_ = std::min(selectedTileset_, static_cast<int>(sprite_.tilesets().size()) - 1);
    selectedTilemap_ = std::min(selectedTilemap_, static_cast<int>(sprite_.tilemaps().size()) - 1);
    tilesRevision_++;
    return true;
}

bool Editor::addTilemap(int tileset, int columns, int rows)
{
    if (stroking() || tileset < 0 || tileset >= static_cast<int>(sprite_.tilesets().size()) ||
        columns < 1 || columns > TILEMAP_MAX_DIMENSION || rows < 1 ||
        rows > TILEMAP_MAX_DIMENSION) {
        return false;
    }
    std::string name = freshName("Map", sprite_.tilemaps(),
                                 [](const TileMap &m) -> const std::string & { return m.name; });
    bool done = false;
    guarded([&] {
        done = tilemap_op_add(history_, sprite_, tilemap_blank(name, tileset, columns, rows));
    });
    if (done) {
        selectedTilemap_ = static_cast<int>(sprite_.tilemaps().size()) - 1;
        tilesRevision_++;
    }
    return done;
}

bool Editor::removeTilemap(int index)
{
    if (stroking() || !tilemap_op_remove(history_, sprite_, index)) {
        return false;
    }
    selectedTilemap_ = std::min(selectedTilemap_, static_cast<int>(sprite_.tilemaps().size()) - 1);
    tilesRevision_++;
    return true;
}

bool Editor::resizeTilemap(int index, int columns, int rows)
{
    bool done = false;
    if (!stroking()) {
        guarded([&] { done = tilemap_op_resize(history_, sprite_, index, columns, rows); });
    }
    tilesRevision_ += done ? 1 : 0;
    return done;
}

bool Editor::setTilemapVisible(int index, bool visible)
{
    if (stroking() || index < 0 || index >= static_cast<int>(sprite_.tilemaps().size())) {
        return false;
    }
    TileMap map = sprite_.tilemaps()[static_cast<size_t>(index)];
    map.visible = visible;
    tilesRevision_++;
    return tilemap_op_set(history_, sprite_, index, std::move(map));
}

bool Editor::renameTilemap(int index, const std::string &name)
{
    if (stroking() || name.empty() || index < 0 ||
        index >= static_cast<int>(sprite_.tilemaps().size())) {
        return false;
    }
    TileMap map = sprite_.tilemaps()[static_cast<size_t>(index)];
    map.name = name;
    tilesRevision_++;
    return tilemap_op_set(history_, sprite_, index, std::move(map));
}

std::unique_ptr<TileStroke> Editor::beginTileStroke()
{
    int map = selectedTilemap();
    return stroking() || map < 0 ? nullptr : TileStroke::begin(history_, sprite_, map);
}

bool Editor::paintTile(int x, int y, uint32_t cell)
{
    if (!tileStroke_) {
        tileStroke_ = beginTileStroke();
    }
    if (!tileStroke_ || !tileStroke_->set(x, y, cell)) {
        return false;
    }
    tilesRevision_++;
    return true;
}

bool Editor::endTilePaint()
{
    if (!tileStroke_) {
        return false;
    }
    bool done = tileStroke_->end();
    tileStroke_.reset();
    tilesRevision_++;
    return done;
}
