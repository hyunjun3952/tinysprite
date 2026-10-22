/* App: what the menu commands do (their labels and keys are in commands.cpp). */

#include "app.hpp"

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
    auto canMerge = [](const App &app) {
        return !app.editor_->stroking() && app.editor_->canMergeLayers();
    };
    auto canMoveUp = [](const App &app) {
        return !app.editor_->stroking() && app.editor_->canMoveLayersUp();
    };
    auto canMoveDown = [](const App &app) {
        return !app.editor_->stroking() && app.editor_->canMoveLayersDown();
    };
    auto hasTabs = [](const App &app) { return !app.editor_->stroking() && app.docs_.size() > 1; };
    static const CommandAction TABLE[] = {
        {[](App &app) { app.openNew_ = true; }, idle},
        {[](App &app) { app.askForPath(FileAction::Open); }, idle},
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
        {[](App &app) { app.editor_->overlays().timeline = !app.editor_->overlays().timeline; },
         always},
        {[](App &app) { app.editor_->addLayer(); }, idle},
        {[](App &app) { app.editor_->duplicateSelectedLayers(); }, idle},
        {[](App &app) { app.editor_->mergeSelectedLayers(); }, canMerge},
        {[](App &app) { app.editor_->moveSelectedLayersBy(1); }, canMoveUp},
        {[](App &app) { app.editor_->moveSelectedLayersBy(-1); }, canMoveDown},
        {[](App &app) { app.editor_->addFrame(); }, idle},
        {[](App &app) { app.editor_->duplicateFrame(); }, idle},
        {[](App &app) { app.closeDocument(app.active_); }, idle},
        {[](App &app) {
             app.activateDocument((app.active_ + 1) % static_cast<int>(app.docs_.size()));
         },
         hasTabs},
        {[](App &app) {
             const int count = static_cast<int>(app.docs_.size());
             app.activateDocument((app.active_ + count - 1) % count);
         },
         hasTabs},
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

void App::commandItem(CommandId id, bool checked, const char *label)
{
    const Command &command = command_of(id);
    if (ImGui::MenuItem(label != nullptr ? label : command.label, command_hint(command).c_str(),
                        checked, commandEnabled(id))) {
        runCommand(id);
    }
}
