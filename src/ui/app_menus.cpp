/* App: the menu bar and the shortcut help window. */

#include "app.h"

#include <string>

#include "imgui.h"
#include "theme.h"

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

void App::fileMenu()
{
    if (!ImGui::BeginMenu("File")) {
        return;
    }
    bool idle = !editor_->stroking();
    commandItem(CommandId::New);
    commandItem(CommandId::Open);
    openRecentMenu(idle);
    commandItem(CommandId::Save);
    commandItem(CommandId::SaveAs);
    commandItem(CommandId::ExportImage);
    ImGui::Separator();
    if (ImGui::MenuItem("Import palette...", nullptr, false, idle)) {
        askForPath(FileAction::ImportPalette);
    }
    if (ImGui::MenuItem("Export palette...")) {
        askForPath(FileAction::ExportPalette);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Quit")) {
        guardUnsaved([this] { running_ = false; });
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
    commandItem(CommandId::SelectAll);
    ImGui::EndMenu();
}

void App::frameMenu()
{
    if (!ImGui::BeginMenu("Frame")) {
        return;
    }
    bool idle = !editor_->stroking();
    int frame = editor_->frame();
    int count = editor_->sprite().frameCount();
    if (ImGui::MenuItem("New frame", nullptr, false, idle)) {
        editor_->addFrame();
    }
    if (ImGui::MenuItem("Duplicate frame", nullptr, false, idle)) {
        editor_->duplicateFrame();
    }
    if (ImGui::MenuItem("Delete frame", nullptr, false, idle && count > 1)) {
        editor_->removeFrame(frame);
    }
    ImGui::Separator();
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
    ImGui::MenuItem("Timeline", nullptr, &editor_->overlays().timeline);
    theme::tip("The frames and playback strip along the bottom of the window.");
    ImGui::MenuItem("Status line", nullptr, &editor_->overlays().status);
    theme::tip("Show the size, zoom, active layer and hovered cel in the canvas corner.");
    ImGui::EndMenu();
}

void App::menuBar()
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    fileMenu();
    editMenu();
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
