/* App: the menu bar and the shortcut help window. */

#include "app.hpp"

#include <cmath>
#include <cstdio>
#include <string>

#include "imgui.h"
#include "theme.hpp"

namespace
{

constexpr const char *SELECT_FIRST = "Select an area first."; /* why a selection item is greyed */

} // namespace

/* The recent files; picking one asks about unsaved changes first. */
void App::openRecentMenu(bool idle)
{
    const bool enabled = idle && !recent_.empty();
    if (!ImGui::BeginMenu("Open recent", enabled)) {
        theme::why_disabled(enabled, "No files have been opened or saved yet.");
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
        openOrAsk(chosen);
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
    if (!theme::begin_menu("File")) {
        return;
    }
    bool idle = !editor_->stroking();
    commandItem(CommandId::New);
    commandItem(CommandId::Open);
    openRecentMenu(idle);
    ImGui::Separator();
    commandItem(CommandId::Save);
    commandItem(CommandId::SaveAs);
    commandItem(CommandId::CloseTab);
    ImGui::Separator();
    exportMenu(idle);
    paletteMenu(idle);
    ImGui::Separator();
    if (ImGui::MenuItem("Quit")) {
        quit();
    }
    ImGui::EndMenu();
}

/* Flip and turn the selected pixels; the same actions as the buttons of the Tools panel. */
void App::selectionMenu()
{
    bool enabled = editor_->hasSelection() && !editor_->stroking();
    if (!ImGui::BeginMenu("Transform selection", enabled)) {
        theme::why_disabled(enabled, SELECT_FIRST);
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
    if (!theme::begin_menu("Edit")) {
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
    theme::why_disabled(selected, SELECT_FIRST);
    if (ImGui::MenuItem("Clear", "Delete", false, selected)) {
        editor_->deleteSelection();
    }
    theme::tip("Clear the selected pixels. The Delete key does this unless the layer list has "
               "focus.");
    theme::why_disabled(selected, SELECT_FIRST);
    selectionMenu();
    ImGui::EndMenu();
}

/* Add, copy, remove and merge. */
void App::layerEditItems()
{
    commandItem(CommandId::NewLayer);
    commandItem(CommandId::DuplicateLayer);
    const bool removable = editor_->canRemoveSelectedLayers();
    if (ImGui::MenuItem("Delete layer...", nullptr, false, removable)) {
        layersPanel_->askToDelete();
    }
    theme::tip("Delete the selected layers, after asking. The Delete key does this while the layer "
               "list has focus.");
    theme::why_disabled(removable, "A sprite needs at least one layer.");
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
    if (!theme::begin_menu("Layer")) {
        return;
    }
    layerMenuItems();
    ImGui::EndMenu();
}

void App::frameMenu()
{
    if (!theme::begin_menu("Frame")) {
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
    commandItem(CommandId::NewFrame);
    commandItem(CommandId::DuplicateFrame);
    const bool removable = idle && editor_->sprite().frameCount() > 1;
    if (ImGui::MenuItem("Delete frame", nullptr, false, removable)) {
        editor_->removeFrame(editor_->frame());
    }
    theme::why_disabled(removable, "A sprite needs at least one frame.");
}

void App::celLinkItems(bool idle)
{
    int frame = editor_->frame();
    int layer = editor_->activeLayer();
    const Layer *active = editor_->sprite().layer(layer);
    const bool unlocked = active != nullptr && !active->locked();
    const bool canLink = idle && frame > 0 && unlocked;
    if (ImGui::MenuItem("Link cel to previous frame", nullptr, false, canLink)) {
        editor_->linkCelToPrevious();
    }
    theme::why_disabled(canLink, unlocked ? "The first frame has no previous frame."
                                          : "The layer is locked.");
    const bool canUnlink = idle && unlocked && active->celLinked(frame);
    if (ImGui::MenuItem("Unlink cel", nullptr, false, canUnlink)) {
        editor_->unlinkCel(layer, frame);
    }
    theme::why_disabled(canUnlink, unlocked ? "This cel is not linked to another frame."
                                            : "The layer is locked.");
}

void App::frameStepItems(bool idle)
{
    int frame = editor_->frame();
    int count = editor_->sprite().frameCount();
    if (ImGui::MenuItem("Previous frame", ",", false, idle && frame > 0)) {
        editor_->setFrame(frame - 1);
    }
    theme::why_disabled(idle && frame > 0, "This is the first frame.");
    if (ImGui::MenuItem("Next frame", ".", false, idle && frame + 1 < count)) {
        editor_->setFrame(frame + 1);
    }
    theme::why_disabled(idle && frame + 1 < count, "This is the last frame.");
    ImGui::Separator();
    if (ImGui::MenuItem(editor_->playing() ? "Stop" : "Play", "Enter", false, idle)) {
        editor_->togglePlayback();
    }
}

/* The size of the whole UI, for high-resolution displays. */
/* The filters; each opens the Filter dialog for its settings. */
void App::filterMenu()
{
    if (!theme::begin_menu("Filter")) {
        return;
    }
    bool idle = !editor_->stroking() && !editor_->playing();
    const char *playing = "Stop the animation first.";
    if (ImGui::MenuItem("Outline...", nullptr, false, idle)) {
        openFilter(FilterParams::Kind::Outline);
    }
    theme::tip("Draws a line around the shapes on the selected layers.");
    theme::why_disabled(idle, playing);
    if (ImGui::MenuItem("Replace color...", nullptr, false, idle)) {
        openFilter(FilterParams::Kind::ReplaceColor);
    }
    theme::tip("Swaps one exact color for another (a palette swap).");
    theme::why_disabled(idle, playing);
    if (ImGui::MenuItem("Hue / Brightness...", nullptr, false, idle)) {
        openFilter(FilterParams::Kind::HueBrightness);
    }
    theme::tip("Turns the hue and lightens or darkens the colors.");
    theme::why_disabled(idle, playing);
    ImGui::EndMenu();
}

void App::uiScaleMenu()
{
    if (!ImGui::BeginMenu("UI scale")) {
        return;
    }
    static constexpr int PERCENTS[] = {100, 125, 150, 175, 200, 250, 300};
    int current = static_cast<int>(std::lround(theme::scale() * 100.0f));
    const float largest = maxUiScale();
    for (int percent : PERCENTS) {
        char label[16];
        std::snprintf(label, sizeof(label), "%d%%", percent);
        const bool fits = static_cast<float>(percent) / 100.0f <= largest;
        if (ImGui::MenuItem(label, nullptr, percent == current, fits)) {
            setUiScale(static_cast<float>(percent) / 100.0f);
        }
        if (!fits && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
            ImGui::SetTooltip("Too large for this display: the layout needs a bigger window.");
        }
    }
    ImGui::EndMenu();
}

void App::viewMenu()
{
    if (!theme::begin_menu("View")) {
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
    ImGui::MenuItem("Slices", nullptr, &editor_->overlays().slices);
    theme::tip("Outline the slices on the canvas, with their names and pivots.");
    ImGui::MenuItem("Rulers", nullptr, &editor_->overlays().rulers);
    theme::tip("Show rulers. Drag from one to pull out a guide, and back onto it to delete.");
    ImGui::MenuItem("Guides", nullptr, &editor_->overlays().guides);
    theme::tip("Show the guide lines pulled out of the rulers.");
    if (ImGui::MenuItem("Clear guides", nullptr, false, !editor_->guides().empty())) {
        editor_->guides().clear();
        editor_->markModified();
    }
    bool onion = editor_->onionEnabled();
    if (ImGui::MenuItem("Onion skin", nullptr, &onion)) {
        editor_->setOnion(onion, editor_->onionSkin());
    }
    theme::tip("Show the neighbouring frames faintly under the current one.");
    ImGui::Separator();
    const bool canSplit = !editor_->stroking() && docs_.size() > 1;
    const char *needsTwo = "Open a second document to show it beside this one.";
    if (ImGui::MenuItem("Split right", nullptr, split_ && splitSideBySide_, canSplit)) {
        splitView(-1, true);
    }
    theme::why_disabled(canSplit, needsTwo);
    if (ImGui::MenuItem("Split down", nullptr, split_ && !splitSideBySide_, canSplit)) {
        splitView(-1, false);
    }
    theme::why_disabled(canSplit, needsTwo);
    if (ImGui::MenuItem("Close split", nullptr, false, split_)) {
        unsplit();
    }
    theme::why_disabled(split_, "The view is not split.");
    theme::tip("Show two documents side by side. Click a pane to work in it; a tab fills the "
               "active pane.");
    ImGui::Separator();
    commandItem(CommandId::ToggleTimeline, editor_->overlays().timeline);
    theme::tip("The frames and playback strip along the bottom of the window.");
    ImGui::MenuItem("Tool options", nullptr, &editor_->overlays().options_bar);
    theme::tip("The options of the active tool, in a bar above the canvas.");
    ImGui::MenuItem("Tilemap window", nullptr, &editor_->overlays().tilemap_window);
    theme::tip("The window where tiles are picked and painted on the selected tilemap.");
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
    filterMenu();
    viewMenu();
    panels_.windowMenu();
    if (theme::begin_menu("Help")) {
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
        {"K P", "Slice, pan (a left drag moves the view)"},
        {"W Q", "Magic wand, lasso (Shift adds to the selection)"},
        {"Hold middle button", "Eyedropper while held (drag instead to pan)"},
        {"X", "Swap foreground / background colors"},
        {"X, Y, Z / Shift+Z (Tilemap window focused)",
         "Mirror the picked tile left to right, top to bottom, turn it clockwise / back"},
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
        {"Arrow keys (Shift: 8 px)",
         "Move the selected pixels (the whole cel without a selection)"},
        {"Click / Ctrl+click / Shift+click on layers",
         "Select one layer, add or remove one, select a range"},
        {"Delete (layer list focused)", "Delete the selected layers, after asking"},
        {", .", "Previous / next frame"},
        {"Ctrl+Shift+F, Ctrl+Shift+J", "New frame, duplicate frame"},
        {"Ctrl+W, Ctrl+Tab / Ctrl+PgDn, Ctrl+Shift+Tab / Ctrl+PgUp",
         "Close tab, next tab, previous tab"},
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
    ImGui::SetNextWindowSize(ImVec2(theme::px(620.0f), 0), ImGuiCond_Appearing);
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
