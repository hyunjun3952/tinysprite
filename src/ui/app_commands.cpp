/* App: what the menu commands do (their labels and keys are in commands.cpp). */

#include "app.h"

#include "imgui.h"

void App::runCommand(CommandId id)
{
    switch (id) {
    case CommandId::New:
        guardUnsaved([this] { openNew_ = true; });
        break;
    case CommandId::Open:
        guardUnsaved([this] { askForPath(FileAction::Open); });
        break;
    case CommandId::Save:
        save();
        break;
    case CommandId::SaveAs:
        askForPath(FileAction::SaveAs);
        break;
    case CommandId::ExportImage:
        askForPath(FileAction::ExportImage);
        break;
    case CommandId::Undo:
        editor_->undo();
        break;
    case CommandId::Redo:
        editor_->redo();
        break;
    case CommandId::Cut:
        editor_->cutSelection();
        break;
    case CommandId::Copy:
        editor_->copySelection();
        break;
    case CommandId::Paste:
        editor_->paste();
        break;
    case CommandId::SelectAll:
        editor_->selectAll();
        break;
    case CommandId::ZoomIn:
        editor_->zoomStep(1);
        break;
    case CommandId::ZoomOut:
        editor_->zoomStep(-1);
        break;
    case CommandId::Fit:
        editor_->view().needs_fit = true;
        break;
    case CommandId::Count:
        break;
    }
}

bool App::commandEnabled(CommandId id) const
{
    bool idle = !editor_->stroking();
    switch (id) {
    case CommandId::New:
    case CommandId::Open:
    case CommandId::Paste:
    case CommandId::SelectAll:
        return idle;
    case CommandId::Undo:
        return idle && editor_->history().canUndo();
    case CommandId::Redo:
        return idle && editor_->history().canRedo();
    case CommandId::Cut:
    case CommandId::Copy:
        return idle && editor_->hasSelection();
    case CommandId::Save:
    case CommandId::SaveAs:
    case CommandId::ExportImage:
    case CommandId::ZoomIn:
    case CommandId::ZoomOut:
    case CommandId::Fit:
    case CommandId::Count:
        return true;
    }
    return false;
}

void App::commandItem(CommandId id)
{
    const Command &command = command_of(id);
    if (ImGui::MenuItem(command.label, command_hint(command).c_str(), false, commandEnabled(id))) {
        runCommand(id);
    }
}
