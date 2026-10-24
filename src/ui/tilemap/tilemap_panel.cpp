#include "tilemap/tilemap_panel.hpp"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>

#include "imgui.h"
#include "theme.hpp"

namespace
{

constexpr float LIST_ROWS = 4.0f;          /* rows of a list shown before it scrolls */
constexpr float FIELD_NAME_COLUMN = 64.0f; /* the label column of the fields, at scale 1 */

void label(const char *text)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", text);
    ImGui::TableSetColumnIndex(1);
}

void intField(const char *id, int *value, int min, int max, const char *unit)
{
    ImGui::SetNextItemWidth(-FLT_MIN);
    theme::input_int(id, value, 1, 8, unit);
    *value = std::clamp(*value, min, max);
}

/* A scrolling list child of `count` rows (at most LIST_ROWS) to fill with selectables. */
bool beginList(const char *id, size_t count)
{
    const float rows = std::min(static_cast<float>(count), LIST_ROWS);
    const ImVec2 size(0.0f, ImGui::GetTextLineHeightWithSpacing() * rows +
                                ImGui::GetStyle().WindowPadding.y);
    if (ImGui::BeginChild(id, size, ImGuiChildFlags_Borders)) {
        return true;
    }
    ImGui::EndChild();
    return false;
}

} // namespace

TilemapPanel::TilemapPanel(Editor &editor) : editor_(&editor)
{
}

void TilemapPanel::setEditor(Editor &editor)
{
    editor_ = &editor;
    shownMap_ = -1;
    nameActive_ = false;
}

void TilemapPanel::show()
{
    /* Tiles cannot change under a tool that is mid-drag. */
    ImGui::BeginDisabled(editor_->stroking());
    tilesets();
    ImGui::Spacing();
    maps();
    ImGui::Spacing();
    ImGui::EndDisabled();
    if (ImGui::Checkbox("Tilemap window", &editor_->overlays().tilemap_window)) {
        /* the window itself is drawn by the App */
    }
    theme::tip("The window where tiles are picked and painted on the selected map.");
}

void TilemapPanel::tilesets()
{
    ImGui::TextDisabled("Tilesets");
    const std::vector<Tileset> &sets = editor_->sprite().tilesets();
    if (sets.empty()) {
        ImGui::TextDisabled("None yet. Cut one from the cel.");
    } else if (beginList("##tilesets", sets.size())) {
        for (int i = 0; i < static_cast<int>(sets.size()); i++) {
            const Tileset &set = sets[static_cast<size_t>(i)];
            ImGui::PushID(i);
            if (ImGui::Selectable(set.name.c_str(), i == editor_->selectedTileset())) {
                editor_->selectTileset(i);
            }
            char text[32];
            std::snprintf(text, sizeof(text), "%d tiles", set.tileCount());
            ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(text).x);
            ImGui::TextDisabled("%s", text);
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    if (ImGui::BeginTable("##tile_size", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthFixed,
                                theme::px(FIELD_NAME_COLUMN));
        ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
        label("Tile width");
        intField("##tile_w", &tileWidth_, 1, TILE_SIZE_MAX, " px");
        label("Tile height");
        intField("##tile_h", &tileHeight_, 1, TILE_SIZE_MAX, " px");
        ImGui::EndTable();
    }
    const float width = theme::columnWidth(2);
    if (ImGui::Button("From cel", ImVec2(width, 0.0f))) {
        editor_->addTilesetFromCel(tileWidth_, tileHeight_);
    }
    theme::tip("Cuts the selection (or the whole active cel) into tiles of this size.");
    ImGui::SameLine();
    if (theme::actionButton("Delete##tileset",
                            "Remove the selected tileset (not while a map uses it).", width,
                            editor_->selectedTileset() >= 0)) {
        editor_->removeTileset(editor_->selectedTileset());
    }
}

void TilemapPanel::maps()
{
    ImGui::TextDisabled("Tilemaps");
    const std::vector<TileMap> &maps = editor_->sprite().tilemaps();
    if (maps.empty()) {
        ImGui::TextDisabled("None yet.");
    } else if (beginList("##tilemaps", maps.size())) {
        for (int i = 0; i < static_cast<int>(maps.size()); i++) {
            const TileMap &map = maps[static_cast<size_t>(i)];
            ImGui::PushID(i);
            if (ImGui::Selectable(map.name.c_str(), i == editor_->selectedTilemap())) {
                editor_->selectTilemap(i);
            }
            char text[32];
            std::snprintf(text, sizeof(text), "%d\xC3\x97%d", map.columns, map.rows);
            ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(text).x);
            ImGui::TextDisabled("%s", text);
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    if (ImGui::BeginTable("##map_size", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthFixed,
                                theme::px(FIELD_NAME_COLUMN));
        ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
        label("Columns");
        intField("##new_cols", &newColumns_, 1, TILEMAP_MAX_DIMENSION, " tiles");
        label("Rows");
        intField("##new_rows", &newRows_, 1, TILEMAP_MAX_DIMENSION, " tiles");
        ImGui::EndTable();
    }
    const float width = theme::columnWidth(2);
    const bool canAdd = editor_->selectedTileset() >= 0;
    if (theme::actionButton("New##map", "A blank map of the selected tileset.", width, canAdd)) {
        editor_->addTilemap(editor_->selectedTileset(), newColumns_, newRows_);
    }
    ImGui::SameLine();
    const int selected = editor_->selectedTilemap();
    if (theme::actionButton("Delete##map", "Remove the selected tilemap.", width, selected >= 0)) {
        editor_->removeTilemap(selected);
    }
    if (selected >= 0) {
        details(selected);
    }
}

void TilemapPanel::details(int index)
{
    const TileMap &map = editor_->sprite().tilemaps()[static_cast<size_t>(index)];
    if (shownMap_ != index || shownRevision_ != editor_->tilesRevision()) {
        if (!nameActive_) {
            columns_ = map.columns;
            rows_ = map.rows;
        }
        shownMap_ = index;
        shownRevision_ = editor_->tilesRevision();
    }
    if (!ImGui::BeginTable("##map_details", 2, ImGuiTableFlags_SizingFixedFit)) {
        return;
    }
    ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthFixed,
                            theme::px(FIELD_NAME_COLUMN));
    ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
    label("Name");
    if (!nameActive_) {
        std::snprintf(name_, sizeof(name_), "%s", map.name.c_str());
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##map_name", name_, sizeof(name_));
    nameActive_ = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        editor_->renameTilemap(index, name_);
    }
    label("Visible");
    bool visible = map.visible;
    if (ImGui::Checkbox("##map_visible", &visible)) {
        editor_->setTilemapVisible(index, visible);
    }
    label("Columns");
    intField("##map_cols", &columns_, 1, TILEMAP_MAX_DIMENSION, " tiles");
    label("Rows");
    intField("##map_rows", &rows_, 1, TILEMAP_MAX_DIMENSION, " tiles");
    ImGui::EndTable();
    const bool changed = columns_ != map.columns || rows_ != map.rows;
    if (theme::actionButton("Resize", "Change the grid; cells that stay inside are kept.",
                            theme::columnWidth(1), changed)) {
        editor_->resizeTilemap(index, columns_, rows_);
    }
}
