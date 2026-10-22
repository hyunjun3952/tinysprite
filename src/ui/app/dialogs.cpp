/* App: the New, Recover and unsaved-changes dialogs, and dropped files. */

#include "app.hpp"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>

#include "imgui.h"
#include "theme.hpp"

void App::guardUnsaved(std::function<void()> proceed)
{
    if (!dirty()) {
        proceed();
        return;
    }
    pending_ = std::move(proceed);
    openConfirm_ = true;
}

/* Save, then go on with what the dialog guarded. Without a path (or after a failed save) the
 * Save as dialog is open: go on from there. */
void App::saveAndProceed()
{
    std::function<void()> proceed = std::move(pending_);
    pending_ = nullptr;
    if (!filePath().empty()) {
        save();
    }
    if (!dirty()) {
        proceed();
        return;
    }
    if (filePath().empty()) {
        askForPath(FileAction::SaveAs);
    }
    afterSave_ = std::move(proceed);
}

void App::confirmDialog()
{
    static const char *const TITLE = "Unsaved changes";
    if (openConfirm_) {
        ImGui::OpenPopup(TITLE);
        openConfirm_ = false;
    }
    confirmShown_ = theme::begin_modal(TITLE);
    if (!confirmShown_) {
        return;
    }
    ImGui::TextUnformatted("The sprite has changes that are not saved.");
    bool close = false;
    const int clicked = theme::dialog_buttons({"Discard", "Cancel", "Save"}, 2);
    if (clicked == 2) {
        close = true;
        saveAndProceed();
    } else if (clicked == 0) {
        close = true;
        std::function<void()> proceed = std::move(pending_);
        pending_ = nullptr;
        proceed();
    }
    if (clicked == 1 || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close = true;
    }
    if (close) {
        pending_ = nullptr;
        ImGui::CloseCurrentPopup();
    }
    theme::end_modal();
}

/* A file dropped on the window opens in a tab; a failure shows in the Open dialog with
 * the dropped path filled in. */
void App::dropFile(const std::string &path)
{
    if (editor_->stroking()) {
        return;
    }
    openOrAsk(path);
}

void App::openOrAsk(const std::string &path)
{
    if (!openPath(path)) {
        std::string error = fileError_;
        askForPath(FileAction::Open);
        fileError_ = error;
        std::snprintf(pathInput_, sizeof(pathInput_), "%s", path.c_str());
    }
}

/* "New sprite": a size, then a blank document. */
/* The size presets and the width / height fields of the New dialog. */
void App::newSizeFields()
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Presets");
    for (int size : {16, 32, 64, 128, 256}) {
        ImGui::SameLine();
        ImGui::PushID(size);
        if (ImGui::Button(std::to_string(size).c_str())) {
            newWidth_ = size;
            newHeight_ = size;
        }
        ImGui::PopID();
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Width ");
    ImGui::SameLine();
    theme::input_int("##new_width", &newWidth_, 1, 16, " px");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Height");
    ImGui::SameLine();
    theme::input_int("##new_height", &newHeight_, 1, 16, " px");
    newWidth_ = std::clamp(newWidth_, 1, CEL_MAX_DIMENSION);
    newHeight_ = std::clamp(newHeight_, 1, CEL_MAX_DIMENSION);
}

void App::newDialog()
{
    static const char *const TITLE = "New sprite";
    if (openNew_) {
        ImGui::OpenPopup(TITLE);
        openNew_ = false;
    }
    newShown_ = theme::begin_modal(TITLE);
    if (!newShown_) {
        return;
    }
    newSizeFields();
    bool close = false;
    const int clicked = theme::dialog_buttons({"Cancel", "Create"}, 1);
    if (clicked == 1 || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        if (addBlankDocument(newWidth_, newHeight_)) {
            close = true;
        }
    }
    if (clicked == 0 || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close = true;
    }
    if (close) {
        ImGui::CloseCurrentPopup();
    }
    theme::end_modal();
}

/* Offered at start when the last session left an autosave copy behind (it did not exit cleanly). */
void App::recoverDialog()
{
    static const char *const TITLE = "Recover unsaved work";
    if (openRecover_) {
        ImGui::OpenPopup(TITLE);
        openRecover_ = false;
    }
    recoverShown_ = theme::begin_modal(TITLE);
    if (!recoverShown_) {
        return;
    }
    ImGui::TextUnformatted("tinysprite did not exit cleanly last time.");
    if (recovered_) {
        ImGui::Text("Recover the unsaved changes to %s?",
                    recovered_->path.empty() ? "an untitled sprite" : recovered_->path.c_str());
    }
    bool close = false;
    const int clicked = theme::dialog_buttons({"Discard", "Recover"}, 1);
    if (clicked == 1) {
        if (recovered_ && addDocument(std::move(recovered_->sprite), recovered_->path, true)) {
            editor_->markModified();
        }
        close = true;
    } else if (clicked == 0) {
        close = true;
    }
    if (close) {
        recovered_.reset();
        recovery_clear(configDir_);
        ImGui::CloseCurrentPopup();
    }
    theme::end_modal();
}
