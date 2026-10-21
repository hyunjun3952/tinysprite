/* App: the New, Recover and unsaved-changes dialogs, and dropped files. */

#include "app.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>

#include "imgui.h"

void App::guardUnsaved(std::function<void()> proceed)
{
    if (!dirty()) {
        proceed();
        return;
    }
    pending_ = std::move(proceed);
    openConfirm_ = true;
}

void App::confirmDialog()
{
    static const char *const TITLE = "Unsaved changes";
    if (openConfirm_) {
        ImGui::OpenPopup(TITLE);
        openConfirm_ = false;
    }
    if (!ImGui::BeginPopupModal(TITLE, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::TextUnformatted("The sprite has changes that are not saved.");
    bool close = false;
    if (ImGui::Button("Save")) {
        close = true;
        std::function<void()> proceed = std::move(pending_);
        pending_ = nullptr;
        if (!filePath_.empty()) {
            save();
        }
        /* Without a path (or after a failed save) the Save as dialog is open: go on from there. */
        if (dirty()) {
            if (filePath_.empty()) {
                askForPath(FileAction::SaveAs);
            }
            afterSave_ = std::move(proceed);
        } else {
            proceed();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
        close = true;
        std::function<void()> proceed = std::move(pending_);
        pending_ = nullptr;
        proceed();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close = true;
    }
    if (close) {
        pending_ = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

/* A file dropped on the window replaces the document; a failure shows in the Open dialog with
 * the dropped path filled in. */
void App::dropFile(const std::string &path)
{
    if (editor_->stroking()) {
        return;
    }
    guardUnsaved([this, path] { openOrAsk(path); });
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
void App::newDialog()
{
    static const char *const TITLE = "New sprite";
    if (openNew_) {
        ImGui::OpenPopup(TITLE);
        openNew_ = false;
    }
    newShown_ = ImGui::BeginPopupModal(TITLE, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (!newShown_) {
        return;
    }
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
    ImGui::InputInt("##new_width", &newWidth_, 1, 16);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Height");
    ImGui::SameLine();
    ImGui::InputInt("##new_height", &newHeight_, 1, 16);
    newWidth_ = std::clamp(newWidth_, 1, CEL_MAX_DIMENSION);
    newHeight_ = std::clamp(newHeight_, 1, CEL_MAX_DIMENSION);
    bool close = false;
    if (ImGui::Button("Create") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        if (editor_->newDocument(newWidth_, newHeight_)) {
            filePath_.clear();
            close = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close = true;
    }
    if (close) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

/* Offered at start when the last session left an autosave copy behind (it did not exit cleanly). */
void App::recoverDialog()
{
    static const char *const TITLE = "Recover unsaved work";
    if (openRecover_) {
        ImGui::OpenPopup(TITLE);
        openRecover_ = false;
    }
    recoverShown_ = ImGui::BeginPopupModal(TITLE, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (!recoverShown_) {
        return;
    }
    ImGui::TextUnformatted("tinysprite did not exit cleanly last time.");
    if (recovered_) {
        ImGui::Text("Recover the unsaved changes to %s?",
                    recovered_->path.empty() ? "an untitled sprite" : recovered_->path.c_str());
    }
    bool close = false;
    if (ImGui::Button("Recover")) {
        if (recovered_ && editor_->setSprite(std::move(recovered_->sprite))) {
            filePath_ = recovered_->path;
            editor_->markModified();
        }
        close = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
        close = true;
    }
    if (close) {
        recovered_.reset();
        recovery_clear(configDir_);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
