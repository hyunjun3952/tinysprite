/* App: keyboard shortcuts. */

#include "app.hpp"

#include <algorithm>
#include <initializer_list>

#include "imgui.h"

/* Whether any of the keys went down this frame (`repeat`: also while held). */
static bool anyPressed(std::initializer_list<ImGuiKey> keys, bool repeat = true)
{
    return std::any_of(keys.begin(), keys.end(),
                       [repeat](ImGuiKey key) { return ImGui::IsKeyPressed(key, repeat); });
}

/* Whether the keyboard is driving the panels (a focus ring is showing) or a popup is open: the
 * arrow keys and Enter then belong to the control, not to the canvas. */
bool App::keyboardOnControls()
{
    const ImGuiIO &io = ImGui::GetIO();
    return (io.NavActive && io.NavVisible) ||
           ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}

/* The arrow keys move the selected pixels by 1 px (8 px with Shift); they repeat while held. Not
 * while the keyboard is on the controls (`controlsHaveKeys`). */
void App::nudgeShortcut(bool controlsHaveKeys)
{
    if (controlsHaveKeys) {
        return;
    }
    const int step = ImGui::GetIO().KeyShift ? 8 : 1;
    int dx = 0;
    int dy = 0;
    if (anyPressed({ImGuiKey_LeftArrow})) {
        dx -= step;
    }
    if (anyPressed({ImGuiKey_RightArrow})) {
        dx += step;
    }
    if (anyPressed({ImGuiKey_UpArrow})) {
        dy -= step;
    }
    if (anyPressed({ImGuiKey_DownArrow})) {
        dy += step;
    }
    editor_->nudgeSelection(dx, dy);
}

/* Keys that act without a modifier: tool letters, X (swap colors), + / - and [ ] (brush size) and F
 * (fit), , . and Enter
 * (frames and playback). */
void App::plainShortcuts(bool controlsHaveKeys)
{
    for (int tool = 0; tool < TOOL_COUNT; tool++) {
        if (ImGui::IsKeyPressed(ToolsPanel::shortcutKey(static_cast<ToolKind>(tool)), false)) {
            editor_->selectTool(static_cast<ToolKind>(tool));
        }
    }
    if (anyPressed({ImGuiKey_X}, false) && !tilemapWindow_.focused()) {
        editor_->swapColors();
    }
    /* + / - (and [ / ]) change the brush size by 1; the wheel zooms. "+" is Shift+= on many
     * layouts, so the Shift state is deliberately ignored. */
    if (anyPressed({ImGuiKey_Equal, ImGuiKey_KeypadAdd, ImGuiKey_RightBracket})) {
        editor_->changeBrushSize(1);
    }
    if (anyPressed({ImGuiKey_Minus, ImGuiKey_KeypadSubtract, ImGuiKey_LeftBracket})) {
        editor_->changeBrushSize(-1);
    }
    if (anyPressed({ImGuiKey_F}, false)) {
        editor_->view().needs_fit = true;
    }
    /* , and . step through the frames (they repeat while held); Enter plays and stops. */
    if (anyPressed({ImGuiKey_Comma})) {
        editor_->setFrame(editor_->frame() - 1);
    }
    if (anyPressed({ImGuiKey_Period})) {
        editor_->setFrame(editor_->frame() + 1);
    }
    nudgeShortcut(controlsHaveKeys);
    if (!controlsHaveKeys && anyPressed({ImGuiKey_Enter, ImGuiKey_KeypadEnter}, false)) {
        editor_->togglePlayback();
    }
}

/* The file / unsaved-changes dialogs are modal: nothing behind them may react. */
bool App::modalOpen() const
{
    return fileAction_ != FileAction::None || confirmShown_ || openConfirm_ || openNew_ ||
           openRecover_ || newShown_ || openFilter_ || filterShown_ || recoverShown_;
}

/* Ctrl+key shortcuts: the commands of command_table.hpp (file, zoom, undo / redo, the clipboard).
 */
void App::ctrlShortcuts()
{
    const Command *command = find_command(
        ImGui::GetIO().KeyShift,
        [](ImGuiKey key, bool repeat) { return ImGui::IsKeyPressed(key, repeat); },
        [this](CommandId id) { return commandEnabled(id); });
    if (command) {
        runCommand(command->id);
    }
}

/* Esc cancels the drag in progress, or else clears the selection. */
void App::escapeShortcut()
{
    if (!ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        return;
    }
    if (editor_->stroking()) {
        editor_->cancelDrag();
    } else if (editor_->playing()) {
        editor_->stopPlayback();
    } else {
        editor_->clearSelection();
    }
}

/* F1 or ? toggles the shortcut list. */
void App::helpShortcut()
{
    const ImGuiIO &io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false) ||
        (io.KeyShift && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Slash, false))) {
        showHelp_ = !showHelp_;
    }
}

/* With the layer list focused Delete means "delete the layer" (after asking); anywhere else it
 * clears the selected pixels. */
void App::deleteShortcut()
{
    if (!ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        return;
    }
    if (layersPanel_->focused()) {
        layersPanel_->askToDelete();
    } else {
        editor_->deleteSelection();
    }
}

void App::handleShortcuts(bool controlsHaveKeys)
{
    const ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput || layersPanel_->dialogOpen() || toolsPanel_->dialogOpen() ||
        modalOpen()) {
        editor_->tempPick(false);
        return;
    }
    helpShortcut();
    escapeShortcut();
    if (editor_->stroking()) {
        return;
    }
    deleteShortcut();
    if (io.KeyCtrl) {
        ctrlShortcuts();
    } else if (!io.KeyAlt) {
        plainShortcuts(controlsHaveKeys);
    }
}
