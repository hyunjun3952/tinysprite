#include "tilemap/tilemap_window.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "imgui.h"
#include "theme.hpp"
#include "tinysprite/raster.hpp"

namespace
{

constexpr float PICKER_HEIGHT = 130.0f; /* at UI scale 1 */

ImTextureRef textureRef(const CelTexture &t)
{
    return {reinterpret_cast<ImTextureID>(t.texture())};
}

void drawGrid(ImDrawList *draw, ImVec2 origin, int columns, int rows, float cellW, float cellH)
{
    const float width = static_cast<float>(columns) * cellW;
    const float height = static_cast<float>(rows) * cellH;
    for (int x = 0; x <= columns; x++) {
        const float px = origin.x + static_cast<float>(x) * cellW;
        draw->AddLine(ImVec2(px, origin.y), ImVec2(px, origin.y + height),
                      theme::overlay::TILE_GRID);
    }
    for (int y = 0; y <= rows; y++) {
        const float py = origin.y + static_cast<float>(y) * cellH;
        draw->AddLine(ImVec2(origin.x, py), ImVec2(origin.x + width, py),
                      theme::overlay::TILE_GRID);
    }
}

} // namespace

void TilemapWindow::release()
{
    atlas_.release();
    picture_.release();
    shownEditor_ = nullptr;
}

/* Uploads the pictures again when the document, the map or any tile has changed since. */
void TilemapWindow::refresh(SDL_Renderer *renderer, const Editor &editor, const Tileset &tileset,
                            const TileMap &map)
{
    const int index = editor.selectedTilemap();
    if (shownEditor_ == &editor && shownRevision_ == editor.tilesRevision() && shownMap_ == index &&
        shownAtlas_ == tileset.atlas.get()) {
        return;
    }
    shownEditor_ = &editor;
    shownRevision_ = editor.tilesRevision();
    shownMap_ = index;
    shownAtlas_ = tileset.atlas.get();
    atlas_.set(renderer, *tileset.atlas);
    std::optional<Cel> picture = tilemap_render(tileset, map);
    if (picture) {
        picture_.set(renderer, *picture);
    } else {
        picture_.release();
    }
}

void TilemapWindow::toolbar(Editor &editor, const TileMap &map)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s  %d\xC3\x97%d", map.name.c_str(), map.columns, map.rows);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(theme::px(110.0f));
    theme::slider_int("##tile_zoom", &zoom_, 1, 12, "x");
    theme::tip("How much the tiles are enlarged.");
    ImGui::SameLine();
    const int selected = editor.selectedTilemap();
    if (ImGui::Button("Fill all")) {
        tilemap_op_fill(editor.history(), editor.sprite(), selected,
                        Rect{0, 0, map.columns, map.rows}, editor.selectedTile());
        editor.tilesChanged();
    }
    theme::tip("Sets every cell of the map to the picked tile.");
}

void TilemapWindow::picker(Editor &editor, const Tileset &tileset)
{
    const auto zoom = static_cast<float>(zoom_);
    const float cellW = static_cast<float>(tileset.tileWidth) * zoom;
    const float cellH = static_cast<float>(tileset.tileHeight) * zoom;
    const bool eraser = editor.selectedTile() == 0;
    if (ImGui::Selectable("Eraser", eraser, 0, ImVec2(theme::px(70.0f), 0.0f))) {
        editor.selectTile(0);
    }
    theme::tip("Picks the empty tile: painting with it clears cells.");
    if (!ImGui::BeginChild("##tile_picker", ImVec2(0.0f, theme::px(PICKER_HEIGHT)),
                           ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::EndChild();
        return;
    }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const int columns = tileset.columns();
    ImGui::InvisibleButton("##picker_area", ImVec2(cellW * static_cast<float>(columns),
                                                   cellH * static_cast<float>(tileset.rows())));
    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->AddImage(textureRef(atlas_), origin,
                   ImVec2(origin.x + cellW * static_cast<float>(columns),
                          origin.y + cellH * static_cast<float>(tileset.rows())));
    drawGrid(draw, origin, columns, tileset.rows(), cellW, cellH);
    if (ImGui::IsItemClicked(0)) {
        const ImVec2 mouse = ImGui::GetMousePos();
        const int tx = static_cast<int>(std::floor((mouse.x - origin.x) / cellW));
        const int ty = static_cast<int>(std::floor((mouse.y - origin.y) / cellH));
        editor.selectTile(static_cast<uint32_t>(ty * columns + tx) + 1);
    }
    const uint32_t picked = editor.selectedTile();
    if (picked > 0) {
        const int tile = static_cast<int>(picked) - 1;
        const int tileColumn = tile % columns;
        const int tileRow = tile / columns;
        const ImVec2 a(origin.x + static_cast<float>(tileColumn) * cellW,
                       origin.y + static_cast<float>(tileRow) * cellH);
        draw->AddRect(a, ImVec2(a.x + cellW, a.y + cellH), theme::overlay::TILE_PICKED, 0.0f, 0,
                      2.0f);
    }
    ImGui::EndChild();
}

/* Paints (or, with the right button, erases) the cells under the mouse while a button is held on
 * the map, joining the steps of a fast drag with a line. */
void TilemapWindow::paint(Editor &editor, const Tileset &tileset, const TileMap &map, float x0,
                          float y0)
{
    const bool left = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool right = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if (!ImGui::IsItemActive() || (!left && !right)) {
        if (editor.tilePainting()) {
            editor.endTilePaint();
        }
        hasLast_ = false;
        return;
    }
    const auto zoom = static_cast<float>(zoom_);
    const ImVec2 mouse = ImGui::GetMousePos();
    const int cx = static_cast<int>(
        std::floor((mouse.x - x0) / (static_cast<float>(tileset.tileWidth) * zoom)));
    const int cy = static_cast<int>(
        std::floor((mouse.y - y0) / (static_cast<float>(tileset.tileHeight) * zoom)));
    const uint32_t cell = left ? editor.selectedTile() : 0;
    const int fromX = hasLast_ ? lastX_ : cx;
    const int fromY = hasLast_ ? lastY_ : cy;
    raster_line(fromX, fromY, cx, cy, [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < map.columns && y < map.rows) {
            editor.paintTile(x, y, cell);
        }
    });
    hasLast_ = true;
    lastX_ = cx;
    lastY_ = cy;
}

void TilemapWindow::mapView(Editor &editor, const Tileset &tileset, const TileMap &map)
{
    if (!ImGui::BeginChild("##tile_map", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                           ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::EndChild();
        return;
    }
    const auto zoom = static_cast<float>(zoom_);
    const float cellW = static_cast<float>(tileset.tileWidth) * zoom;
    const float cellH = static_cast<float>(tileset.tileHeight) * zoom;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size(cellW * static_cast<float>(map.columns),
                      cellH * static_cast<float>(map.rows));
    ImGui::InvisibleButton("##map_area", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                        ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::CANVAS_SOFT)));
    if (picture_.texture()) {
        draw->AddImage(textureRef(picture_), origin, ImVec2(origin.x + size.x, origin.y + size.y));
    }
    drawGrid(draw, origin, map.columns, map.rows, cellW, cellH);
    paint(editor, tileset, map, origin.x, origin.y);
    ImGui::EndChild();
}

void TilemapWindow::show(SDL_Renderer *renderer, Editor &editor)
{
    bool &open = editor.overlays().tilemap_window;
    if (!open) {
        if (editor.tilePainting()) {
            editor.endTilePaint();
        }
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(theme::px(520.0f), theme::px(460.0f)), ImGuiCond_FirstUseEver);
    const bool visible = ImGui::Begin("Tilemap", &open);
    const int index = editor.selectedTilemap();
    if (visible && index < 0) {
        ImGui::TextWrapped("Cut a tileset from the cel and make a tilemap in the Tilemaps section "
                           "of the Inspector, then pick it there.");
    } else if (visible) {
        const TileMap &map = editor.sprite().tilemaps()[static_cast<size_t>(index)];
        const Tileset &tileset = editor.sprite().tilesets()[static_cast<size_t>(map.tileset)];
        refresh(renderer, editor, tileset, map);
        toolbar(editor, map);
        picker(editor, tileset);
        mapView(editor, tileset, map);
    }
    ImGui::End();
}
