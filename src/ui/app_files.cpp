/* App: opening, saving and exporting files, and the path dialog. */

#include "app.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
    case FileAction::ExportGif: {
        GifOptions options;
        exportRange(options.first, options.last, options.direction);
        options.scale = exportScale_;
        options.loop = gifLoop_;
        if (!gif_export(editor_->sprite(), path, options)) {
            fileError_ = "Cannot write " + path + " (use a .gif file name; up to 16384 px)";
            return false;
        }
        return true;
    }
    case FileAction::ExportSheet: {
        SheetOptions options;
        PlayMode unused = PlayMode::Forward;
        exportRange(options.first, options.last, unused);
        options.layout = static_cast<SheetLayout>(sheetLayout_);
        options.columns = sheetColumns_;
        options.spacing = sheetSpacing_;
        options.scale = exportScale_;
        if (!spritesheet_export(editor_->sprite(), path, options)) {
            fileError_ = "Cannot write " + path +
                         " (use .png, .bmp, .tga or .jpg; the sheet can be up to 16384 px)";
            return false;
        }
        return true;
    }
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

void App::exportRange(int &first, int &last, PlayMode &direction) const
{
    const std::vector<Sprite::Tag> &tags = editor_->sprite().tags();
    first = 0;
    last = -1;
    direction = PlayMode::Forward;
    if (exportTag_ >= 0 && exportTag_ < static_cast<int>(tags.size())) {
        const Sprite::Tag &tag = tags[static_cast<size_t>(exportTag_)];
        first = tag.from;
        last = tag.to;
        direction = tag.direction;
    }
}

/* The fields of the animation exports: which frames, the enlargement and the format's own
 * options. */
void App::exportOptions()
{
    const bool gif = fileAction_ == FileAction::ExportGif;
    const std::vector<Sprite::Tag> &tags = editor_->sprite().tags();
    if (exportTag_ >= static_cast<int>(tags.size())) {
        exportTag_ = -1;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Frames");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    std::string current =
        exportTag_ < 0 ? "All frames" : tags[static_cast<size_t>(exportTag_)].name;
    if (ImGui::BeginCombo("##export_range", current.c_str())) {
        if (ImGui::Selectable("All frames", exportTag_ < 0)) {
            exportTag_ = -1;
        }
        for (int t = 0; t < static_cast<int>(tags.size()); t++) {
            ImGui::PushID(t);
            if (ImGui::Selectable(tags[static_cast<size_t>(t)].name.c_str(), t == exportTag_)) {
                exportTag_ = t;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Scale");
    ImGui::SameLine();
    ImGui::InputInt("##export_scale", &exportScale_, 1, 2);
    exportScale_ = std::clamp(exportScale_, 1, IMAGE_EXPORT_MAX_SCALE);
    if (gif) {
        ImGui::Checkbox("Loop forever", &gifLoop_);
        return;
    }
    static const char *const LAYOUTS[] = {"Horizontal strip", "Vertical strip", "Grid"};
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Layout");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##sheet_layout", LAYOUTS[sheetLayout_])) {
        for (int i = 0; i < 3; i++) {
            if (ImGui::Selectable(LAYOUTS[i], i == sheetLayout_)) {
                sheetLayout_ = i;
            }
        }
        ImGui::EndCombo();
    }
    if (sheetLayout_ == static_cast<int>(SheetLayout::Grid)) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Columns");
        ImGui::SameLine();
        ImGui::InputInt("##sheet_columns", &sheetColumns_, 1, 2);
        sheetColumns_ = std::clamp(sheetColumns_, 0, 256);
        ImGui::SameLine();
        ImGui::TextDisabled("0 = square-ish");
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Spacing");
    ImGui::SameLine();
    ImGui::InputInt("##sheet_spacing", &sheetSpacing_, 1, 4);
    sheetSpacing_ = std::clamp(sheetSpacing_, 0, 256);
}

void App::fileDialog()
{
    static const char *const TITLES[] = {"",
                                         "Open",
                                         "Save as",
                                         "Export image",
                                         "Import palette",
                                         "Export palette",
                                         "Export GIF",
                                         "Export sprite sheet"};
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
    if (fileAction_ == FileAction::ExportGif || fileAction_ == FileAction::ExportSheet) {
        exportOptions();
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
