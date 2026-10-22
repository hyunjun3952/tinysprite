/* App: what the menu commands do (their labels and keys are in commands.cpp). */

#include "app.h"

#include <iterator>

#include "imgui.h"

/* What each command does and when it can run, one row per CommandId in the same order as COMMANDS
 * (commands.cpp holds the labels and keys). A new command is a row here, not a case in two
 * switches. */
const App::CommandAction &App::commandAction(CommandId id)
{
    auto always = [](const App &) { return true; };
    auto idle = [](const App &app) { return !app.editor_->stroking(); };
    auto canUndo = [](const App &app) {
        return !app.editor_->stroking() && app.editor_->history().canUndo();
    };
    auto canRedo = [](const App &app) {
        return !app.editor_->stroking() && app.editor_->history().canRedo();
    };
    auto selected = [](const App &app) {
        return !app.editor_->stroking() && app.editor_->hasSelection();
    };
    static const CommandAction TABLE[] = {
        {[](App &app) { app.guardUnsaved([&app] { app.openNew_ = true; }); }, idle},
        {[](App &app) { app.guardUnsaved([&app] { app.askForPath(FileAction::Open); }); }, idle},
        {[](App &app) { app.save(); }, always},
        {[](App &app) { app.askForPath(FileAction::SaveAs); }, always},
        {[](App &app) { app.askForPath(FileAction::ExportImage); }, always},
        {[](App &app) { app.editor_->undo(); }, canUndo},
        {[](App &app) { app.editor_->redo(); }, canRedo},
        {[](App &app) { app.editor_->cutSelection(); }, selected},
        {[](App &app) { app.editor_->copySelection(); }, selected},
        {[](App &app) { app.editor_->paste(); }, idle},
        {[](App &app) { app.editor_->selectAll(); }, idle},
        {[](App &app) { app.editor_->zoomStep(1); }, always},
        {[](App &app) { app.editor_->zoomStep(-1); }, always},
        {[](App &app) { app.editor_->view().needs_fit = true; }, always},
    };
    static_assert(std::size(TABLE) == static_cast<size_t>(COMMAND_COUNT),
                  "one CommandAction row per CommandId");
    return TABLE[static_cast<size_t>(id)];
}

void App::runCommand(CommandId id)
{
    commandAction(id).run(*this);
}

bool App::commandEnabled(CommandId id) const
{
    return commandAction(id).enabled(*this);
}

void App::commandItem(CommandId id)
{
    const Command &command = command_of(id);
    if (ImGui::MenuItem(command.label, command_hint(command).c_str(), false, commandEnabled(id))) {
        runCommand(id);
    }
}
