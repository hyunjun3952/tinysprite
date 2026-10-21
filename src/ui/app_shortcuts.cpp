/* App: keyboard shortcuts. */

#include "app.h"

#include "imgui.h"

/* Keys that act without a modifier: tool letters, X (swap colors), + / - and [ ] (brush size) and F
 * (fit). */
void App::plainShortcuts()
{
    for (int tool = 0; tool < TOOL_COUNT; tool++) {
        if (ImGui::IsKeyPressed(ToolsPanel::shortcutKey(static_cast<ToolKind>(tool)), false)) {
            editor_->selectTool(static_cast<ToolKind>(tool));
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_X, false)) {
        editor_->swapColors();
    }
    /* + / - (and [ / ]) change the brush size by 1; the wheel zooms. "+" is Shift+= on many
     * layouts, so the Shift state is deliberately ignored. */
    if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd) ||
        ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
        editor_->changeBrushSize(1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract) ||
        ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
        editor_->changeBrushSize(-1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        editor_->view().needs_fit = true;
    }
}

/* The file / unsaved-changes dialogs are modal: nothing behind them may react. */
bool App::modalOpen() const
{
    return fileAction_ != FileAction::None || ImGui::IsPopupOpen("Unsaved changes") ||
           openConfirm_ || openNew_ || openRecover_ || newShown_ || recoverShown_;
}

/* Ctrl+key shortcuts: the commands of commands.h (file, zoom, undo / redo, the clipboard). */
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
    } else {
        editor_->clearSelection();
    }
}

void App::handleShortcuts()
{
    const ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput || layersPanel_->dialogOpen() || toolsPanel_->dialogOpen() ||
        modalOpen()) {
        editor_->tempPick(false);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false) ||
        (io.KeyShift && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Slash, false))) {
        showHelp_ = !showHelp_;
    }
    escapeShortcut();
    if (editor_->stroking()) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        /* With the layer list focused Delete means "delete the layer" (after asking); anywhere
         * else it clears the selected pixels. */
        if (layersPanel_->focused()) {
            layersPanel_->askToDelete();
        } else {
            editor_->deleteSelection();
        }
    }
    if (io.KeyCtrl) {
        ctrlShortcuts();
    } else if (!io.KeyAlt) {
        plainShortcuts();
    }
}
