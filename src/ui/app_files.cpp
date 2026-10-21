/* App: opening, saving and exporting files, and the path dialog. */

#include "app.h"

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

#include "imgui.h"
#include "tinysprite/io.h"

void App::askForPath(FileAction action)
{
    fileAction_ = action;
    openFileDialog_ = true;
    fileError_.clear();
    std::snprintf(pathInput_, sizeof(pathInput_), "%s",
                  action == FileAction::SaveAs ? filePath_.c_str() : "");
}

void App::save()
{
    if (filePath_.empty()) {
        askForPath(FileAction::SaveAs);
    } else if (!pxa_save(editor_->sprite(), filePath_)) {
        askForPath(FileAction::SaveAs);
        fileError_ = "Cannot write " + filePath_;
    } else {
        editor_->markSaved();
    }
}

bool App::openPath(const std::string &path)
{
    /* A .pxa file starts with its magic number; anything else may be an image. */
    std::optional<Sprite> sprite = pxa_load(path);
    bool isPxa = sprite.has_value();
    if (!isPxa) {
        sprite = image_import(path);
    }
    if (!sprite) {
        fileError_ = "Cannot read " + path;
    } else if (!editor_->setSprite(std::move(*sprite))) {
        fileError_ = "Cannot open " + path + " right now";
    } else {
        filePath_ = isPxa ? path : "";
        if (isPxa) {
            recent_add(recent_, path, RECENT_MAX);
        }
        return true;
    }
    return false;
}

/* Runs the action the dialog asked for; on failure sets fileError_ and returns false. */
bool App::runFileAction(const std::string &path)
{
    switch (fileAction_) {
    case FileAction::Open:
        return openPath(path);
    case FileAction::SaveAs:
        if (!pxa_save(editor_->sprite(), path)) {
            fileError_ = "Cannot write " + path;
            return false;
        }
        filePath_ = path;
        recent_add(recent_, path, RECENT_MAX);
        editor_->markSaved();
        return true;
    case FileAction::ExportImage:
        if (!image_export(editor_->sprite(), editor_->frame(), path, exportScale_)) {
            fileError_ = "Cannot write " + path +
                         " (use .png, .bmp, .tga or .jpg; the image can be up to 16384 px)";
            return false;
        }
        return true;
    case FileAction::ImportPalette: {
        std::optional<Palette> palette = palette_load(path);
        if (!palette) {
            fileError_ = "Cannot read " + path + " (a .gpl or .hex palette)";
            return false;
        }
        editor_->setPalette(std::move(*palette));
        return true;
    }
    case FileAction::ExportPalette:
        if (!palette_save(editor_->sprite().palette(), path)) {
            fileError_ = "Cannot write " + path + " (use a .gpl or .hex file name)";
            return false;
        }
        return true;
    case FileAction::None:
        break;
    }
    return true;
}

void App::fileDialog()
{
    static const char *const TITLES[] = {
        "", "Open", "Save as", "Export image", "Import palette", "Export palette"};
    const char *title = TITLES[static_cast<int>(fileAction_)];
    if (openFileDialog_) {
        ImGui::OpenPopup(title);
        openFileDialog_ = false;
    }
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Path");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(360.0f);
    bool enter = ImGui::InputText("##path", pathInput_, sizeof(pathInput_),
                                  ImGuiInputTextFlags_EnterReturnsTrue);
    const std::string path = pathInput_;
    if (fileAction_ == FileAction::ExportImage) {
        /* Whole-pixel enlargement, shown as the size it gives. */
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Scale");
        ImGui::SameLine();
        ImGui::InputInt("##export_scale", &exportScale_, 1, 2);
        exportScale_ = std::clamp(exportScale_, 1, IMAGE_EXPORT_MAX_SCALE);
        ImGui::SameLine();
        ImGui::TextDisabled("%d x %d px", editor_->sprite().width() * exportScale_,
                            editor_->sprite().height() * exportScale_);
    }
    if (!fileError_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", fileError_.c_str());
    }
    bool done = false;
    if ((ImGui::Button("OK") || enter) && !path.empty()) {
        done = runFileAction(path);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        done = true;
    }
    if (done) {
        bool saved = fileAction_ == FileAction::SaveAs && !dirty();
        fileAction_ = FileAction::None;
        ImGui::CloseCurrentPopup();
        std::function<void()> proceed = std::move(afterSave_);
        afterSave_ = nullptr;
        if (saved && proceed) {
            proceed();
        }
    }
    ImGui::EndPopup();
}
