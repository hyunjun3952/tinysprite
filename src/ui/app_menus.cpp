/* App: the menu bar and the shortcut help window. */

#include "app.hpp"

#include <cmath>
#include <cstdio>
#include <string>

#include "imgui.h"
#include "theme.hpp"

/* The recent files; picking one asks about unsaved changes first. */
void App::openRecentMenu(bool idle)
{
    if (!ImGui::BeginMenu("Open recent", idle && !recent_.empty())) {
        return;
    }
    std::string chosen;
    for (size_t i = 0; i < recent_.size(); i++) {
        ImGui::PushID(static_cast<int>(i));
        std::string name = recent_[i].substr(recent_[i].find_last_of("/\\") + 1);
        if (ImGui::MenuItem(name.c_str())) {
            chosen = recent_[i];
        }
        theme::tip(recent_[i].c_str());
        ImGui::PopID();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Clear list")) {
        recent_.clear();
    }
    ImGui::EndMenu();
    if (!chosen.empty()) {
        guardUnsaved([this, chosen] { openOrAsk(chosen); });
    }
}

/* The three ways out of the editor as pictures: one frame, the animation, a sheet for a game. */
void App::exportMenu(bool idle)
{
    if (!ImGui::BeginMenu("Export")) {
        return;
    }
    commandItem(CommandId::ExportImage);
    if (ImGui::MenuItem("Animation (GIF)...", nullptr, false, idle)) {
        askForPath(FileAction::ExportGif);
    }
    if (ImGui::MenuItem("Sprite sheet...", nullptr, false, idle)) {
        askForPath(FileAction::ExportSheet);
    }
    ImGui::EndMenu();
}

void App::paletteMenu(bool idle)
{
    if (!ImGui::BeginMenu("Palette")) {
        return;
    }
    if (ImGui::MenuItem("Import...", nullptr, false, idle)) {
        askForPath(FileAction::ImportPalette);
    }
    if (ImGui::MenuItem("Export...", nullptr, false, idle)) {
        askForPath(FileAction::ExportPalette);
    }
    ImGui::EndMenu();
}

void App::fileMenu()
{
    if (!ImGui::BeginMenu("File")) {
        return;
    }
    bool idle = !editor_->stroking();
    commandItem(CommandId::New);
    commandItem(CommandId::Open);
    openRecentMenu(idle);
    ImGui::Separator();
    commandItem(CommandId::Save);
    commandItem(CommandId::SaveAs);
    ImGui::Separator();
    exportMenu(idle);
    paletteMenu(idle);
    ImGui::Separator();
    if (ImGui::MenuItem("Quit")) {
        guardUnsaved([this] { running_ = false; });
    }
    ImGui::EndMenu();
}

/* Flip and turn the selected pixels; the same actions as the buttons of the Tools panel. */
void App::selectionMenu()
{
    bool enabled = editor_->hasSelection() && !editor_->stroking();
    if (!ImGui::BeginMenu("Transform selection", enabled)) {
        return;
    }
    if (ImGui::MenuItem("Flip horizontally")) {
        editor_->flipSelection(true);
    }
    if (ImGui::MenuItem("Flip vertically")) {
        editor_->flipSelection(false);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Rotate clockwise")) {
        editor_->rotateSelection(true);
    }
    if (ImGui::MenuItem("Rotate counterclockwise")) {
        editor_->rotateSelection(false);
    }
    ImGui::EndMenu();
}

void App::editMenu()
{
    if (!ImGui::BeginMenu("Edit")) {
        return;
    }
    commandItem(CommandId::Undo);
    commandItem(CommandId::Redo);
    ImGui::Separator();
    commandItem(CommandId::Cut);
    commandItem(CommandId::Copy);
    commandItem(CommandId::Paste);
    ImGui::Separator();
    commandItem(CommandId::SelectAll);
    bool selected = editor_->hasSelection() && !editor_->stroking();
    if (ImGui::MenuItem("Deselect", "Esc", false, selected)) {
        editor_->clearSelection();
    }
    if (ImGui::MenuItem("Clear", "Delete", false, selected)) {
        editor_->deleteSelection();
    }
    selectionMenu();
    ImGui::EndMenu();
}

/* Add, copy, remove and merge. */
void App::layerEditItems()
{
    commandItem(CommandId::NewLayer);
    commandItem(CommandId::DuplicateLayer);
    if (ImGui::MenuItem("Delete layer...", "Delete", false, editor_->canRemoveSelectedLayers())) {
        layersPanel_->askToDelete();
    }
    ImGui::Separator();
    const bool several = editor_->selectedLayers().size() > 1;
    commandItem(CommandId::MergeLayerDown, false, several ? "Merge layers" : nullptr);
}

/* Hide and lock follow the active layer: the label says what the click does to the selection. */
void App::layerStateItems(bool idle)
{
    const Layer *active = editor_->sprite().layer(editor_->activeLayer());
    if (active == nullptr) {
        return;
    }
    if (ImGui::MenuItem(active->visible() ? "Hide layer" : "Show layer", nullptr, false, idle)) {
        editor_->setSelectedLayersVisible(!active->visible());
    }
    if (ImGui::MenuItem(active->locked() ? "Unlock layer" : "Lock layer", nullptr, false, idle)) {
        editor_->setSelectedLayersLocked(!active->locked());
    }
}

/* The layer actions, in the Layer menu and in the context menu of the layer list (right click),
 * which has no buttons of its own: they act on every selected layer. */
void App::layerMenuItems()
{
    const bool idle = !editor_->stroking();
    layerEditItems();
    ImGui::Separator();
    commandItem(CommandId::MoveLayerUp);
    commandItem(CommandId::MoveLayerDown);
    ImGui::Separator();
    layerStateItems(idle);
}

void App::layerMenu()
{
    if (!ImGui::BeginMenu("Layer")) {
        return;
    }
    layerMenuItems();
    ImGui::EndMenu();
}

void App::frameMenu()
{
    if (!ImGui::BeginMenu("Frame")) {
        return;
    }
    bool idle = !editor_->stroking();
    frameEditItems(idle);
    ImGui::Separator();
    celLinkItems(idle);
    ImGui::Separator();
    frameStepItems(idle);
    ImGui::EndMenu();
}

void App::frameEditItems(bool idle)
{
    if (ImGui::MenuItem("New frame", nullptr, false, idle)) {
        editor_->addFrame();
    }
    if (ImGui::MenuItem("Duplicate frame", nullptr, false, idle)) {
        editor_->duplicateFrame();
    }
    if (ImGui::MenuItem("Delete frame", nullptr, false,
                        idle && editor_->sprite().frameCount() > 1)) {
        editor_->removeFrame(editor_->frame());
    }
}

void App::celLinkItems(bool idle)
{
    int frame = editor_->frame();
    int layer = editor_->activeLayer();
    const Layer *active = editor_->sprite().layer(layer);
    if (ImGui::MenuItem("Link cel to previous frame", nullptr, false,
                        idle && frame > 0 && active != nullptr && !active->locked())) {
        editor_->linkCelToPrevious();
    }
    if (ImGui::MenuItem("Unlink cel", nullptr, false,
                        idle && active != nullptr && !active->locked() &&
                            active->celLinked(frame))) {
        editor_->unlinkCel(layer, frame);
    }
}

void App::frameStepItems(bool idle)
{
    int frame = editor_->frame();
    int count = editor_->sprite().frameCount();
    if (ImGui::MenuItem("Previous frame", ",", false, idle && frame > 0)) {
        editor_->setFrame(frame - 1);
    }
    if (ImGui::MenuItem("Next frame", ".", false, idle && frame + 1 < count)) {
        editor_->setFrame(frame + 1);
    }
    ImGui::Separator();
    if (ImGui::MenuItem(editor_->playing() ? "Stop" : "Play", "Enter", false, idle)) {
        editor_->togglePlayback();
    }
}

/* The size of the whole UI, for high-resolution displays. */
void App::uiScaleMenu()
{
    if (!ImGui::BeginMenu("UI scale")) {
        return;
    }
    static constexpr int PERCENTS[] = {100, 125, 150, 175, 200, 250, 300};
    int current = static_cast<int>(std::lround(theme::scale() * 100.0f));
    for (int percent : PERCENTS) {
        char label[16];
        std::snprintf(label, sizeof(label), "%d%%", percent);
        if (ImGui::MenuItem(label, nullptr, percent == current)) {
            setUiScale(static_cast<float>(percent) / 100.0f);
        }
    }
    ImGui::EndMenu();
}

void App::viewMenu()
{
    if (!ImGui::BeginMenu("View")) {
        return;
    }
    commandItem(CommandId::ZoomIn);
    commandItem(CommandId::ZoomOut);
    commandItem(CommandId::Fit);
    ImGui::Separator();
    bool grid = editor_->showGrid();
    if (ImGui::MenuItem("Pixel grid", nullptr, &grid)) {
        editor_->setShowGrid(grid);
    }
    ImGui::MenuItem("Cursor outline", nullptr, &editor_->overlays().cursor);
    theme::tip("Outline the cel under the mouse, and the footprint of a big brush.");
    ImGui::MenuItem("Mirror axes", nullptr, &editor_->overlays().mirror_axes);
    theme::tip("Draw the center lines that Mirror X / Y reflect across.");
    bool onion = editor_->onionEnabled();
    if (ImGui::MenuItem("Onion skin", nullptr, &onion)) {
        editor_->setOnion(onion, editor_->onionSkin());
    }
    theme::tip("Show the neighbouring frames faintly under the current one.");
    ImGui::Separator();
    commandItem(CommandId::ToggleTimeline, editor_->overlays().timeline);
    theme::tip("The frames and playback strip along the bottom of the window.");
    ImGui::MenuItem("Tool options", nullptr, &editor_->overlays().options_bar);
    theme::tip("The options of the active tool, in a bar above the canvas.");
    ImGui::MenuItem("Status bar", nullptr, &editor_->overlays().status);
    theme::tip("The bar along the bottom with the size, tool, frame, zoom and hovered cel.");
    ImGui::Separator();
    uiScaleMenu();
    ImGui::EndMenu();
}

void App::menuBar()
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    fileMenu();
    editMenu();
    layerMenu();
    frameMenu();
    viewMenu();
    panels_.windowMenu();
    if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("Keyboard shortcuts", "F1", &showHelp_);
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

/* Every shortcut in one place, since the tooltips only mention the tool letters. */
void App::helpWindow()
{
    if (!showHelp_) {
        return;
    }
    static const char *const ROWS[][2] = {
        {"B E G I L R O M", "Pencil, eraser, bucket, eyedropper, line, rectangle, ellipse, select"},
        {"T D H S C", "Triangle, diamond, hexagon, star, cross (shapes)"},
        {"Hold middle button", "Eyedropper while held (drag instead to pan)"},
        {"X", "Swap foreground / background colors"},
        {"+  -  or  [  ]", "Larger / smaller brush by 1 px"},
        {"Shift + click", "Pencil / eraser: straight line from the last point"},
        {"Left drag", "Draw with the active tool (inside the selection, if there is one)"},
        {"Wheel", "Zoom at the cursor"},
        {"Ctrl + / Ctrl - / Ctrl+0", "Zoom in / out / fit the sprite"},
        {"Shift (while dragging a shape)", "Square box or circle; a line snaps to 45 degrees"},
        {"Right / middle drag, Space + drag", "Pan"},
        {"Mouse wheel, F", "Zoom at the cursor, fit the sprite"},
        {"Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z", "Undo, redo"},
        {"Ctrl+A, Delete, Esc", "Select all, clear the selection's pixels, cancel / deselect"},
        {"Click / Ctrl+click / Shift+click on layers",
         "Select one layer, add or remove one, select a range"},
        {"Delete (layer list focused)", "Delete the selected layers, after asking"},
        {", .", "Previous / next frame"},
        {"Enter", "Play / stop the animation (Esc also stops it)"},
        {"Ctrl+C, Ctrl+X, Ctrl+V", "Copy, cut, paste"},
        {"Right click on the layer list", "Layer actions (the same as the Layer menu)"},
        {"Ctrl+Shift+N, Ctrl+Shift+D, Ctrl+Shift+M", "New layer, duplicate layer, merge down"},
        {"Ctrl+], Ctrl+[", "Move the selected layers up / down"},
        {"Ctrl+T",
         "Show / hide the timeline (or drag its top edge; double-click the edge to reset)"},
        {"Ctrl+N, Ctrl+O", "New, open"},
        {"Ctrl+S, Ctrl+Shift+S, Ctrl+E", "Save, save as, export image"},
        {"F1", "This window"},
    };
    ImGui::SetNextWindowSize(ImVec2(620, 0), ImGuiCond_Appearing);
    if (ImGui::Begin("Keyboard shortcuts", &showHelp_, ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::BeginTable("shortcuts", 2,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            for (const auto &row : ROWS) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row[0]);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row[1]);
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
