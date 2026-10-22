/* App: opening, saving and exporting files, and the path dialog. */

#include "app.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "file_paths.h"
#include "imgui.h"
#include "tinysprite/io.h"

/* One row per FileAction, in enum order: how the dialog is titled, the extension it adds to a name
 * typed without one ("" for the actions that read), what the toast says once it worked, the step
 * that does the work and the dialog fields it needs beyond the path. A new action is a row here and
 * a method, not a case in each of several switches. */
const App::FileActionInfo &App::fileInfo(FileAction action)
{
    static const FileActionInfo TABLE[] = {
        {"", "", "", nullptr, nullptr},
        {"Open", "", "Opened ", &App::runOpen, nullptr},
        {"Save as", ".pxa", "Saved ", &App::runSaveAs, nullptr},
        {"Export image", ".png", "Exported ", &App::runExportImage, &App::imageOptions},
        {"Import palette", "", "Imported palette ", &App::runImportPalette, nullptr},
        {"Export palette", ".gpl", "Exported ", &App::runExportPalette, nullptr},
        {"Export GIF", ".gif", "Exported ", &App::runExportGif, &App::gifOptions},
        {"Export sprite sheet", ".png", "Exported ", &App::runExportSheet, &App::sheetOptions},
    };
    static_assert(std::size(TABLE) == static_cast<size_t>(FileAction::Count),
                  "one FileActionInfo row per FileAction");
    return TABLE[static_cast<size_t>(action)];
}

bool App::FileActionInfo::writes() const
{
    return extension[0] != '\0';
}

void App::askForPath(FileAction action)
{
    fileAction_ = action;
    openFileDialog_ = true;
    fileError_.clear();
    overwritePath_.clear();
    /* Start where the last file was, and offer the document's name for what is written. */
    std::string start;
    if (action == FileAction::SaveAs && !filePath_.empty()) {
        start = filePath_;
    } else if (fileInfo(action).writes()) {
        std::string name = filePath_.empty() ? "sprite" : file_name_of(filePath_);
        size_t dot = name.find_last_of('.');
        start = join_path(lastDir_, (dot == std::string::npos ? name : name.substr(0, dot)) +
                                        fileInfo(action).extension);
    } else {
        start = lastDir_.empty() ? "" : join_path(lastDir_, "");
    }
    std::snprintf(pathInput_, sizeof(pathInput_), "%s", start.c_str());
}

bool App::overwriteAllowed(const std::string &path)
{
    std::error_code error;
    bool exists = std::filesystem::exists(path, error);
    if (!exists || (fileAction_ == FileAction::SaveAs && path == filePath_) ||
        overwritePath_ == path) {
        return true;
    }
    overwritePath_ = path;
    fileError_.clear();
    return false;
}

/* Remembers the folder and confirms what the dialog just did. */
void App::fileDone(const std::string &path)
{
    std::string dir = directory_of(path);
    if (!dir.empty()) {
        lastDir_ = dir;
    }
    toast(std::string(fileInfo(fileAction_).done) + file_name_of(path));
}

void App::save()
{
    if (filePath_.empty()) {
        askForPath(FileAction::SaveAs);
    } else if (!pxa_save(editor_->sprite(), filePath_)) {
        askForPath(FileAction::SaveAs);
        fileError_ = "Cannot write " + filePath_;
        toast(fileError_, true);
    } else {
        editor_->markSaved();
        toast("Saved " + file_name_of(filePath_));
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

/* The steps of the file dialog, one per action: each does the work on `path` and, on failure, sets
 * fileError_ and returns false. */
bool App::runOpen(const std::string &path)
{
    return openPath(path);
}

bool App::runSaveAs(const std::string &path)
{
    if (!pxa_save(editor_->sprite(), path)) {
        fileError_ = "Cannot write " + path;
        return false;
    }
    filePath_ = path;
    recent_add(recent_, path, RECENT_MAX);
    editor_->markSaved();
    return true;
}

bool App::runExportImage(const std::string &path)
{
    if (!image_export(editor_->sprite(), editor_->frame(), path, exportScale_)) {
        fileError_ = "Cannot write " + path + " (use " + image_write_formats() +
                     "; the image can be up to 16384 px)";
        return false;
    }
    return true;
}

bool App::runExportGif(const std::string &path)
{
    GifOptions options;
    exportRange(options.first, options.last, options.direction);
    options.scale = exportScale_;
    options.loop = export_.loop;
    if (!gif_export(editor_->sprite(), path, options)) {
        fileError_ = "Cannot write " + path + " (use a .gif file name; up to 16384 px)";
        return false;
    }
    return true;
}

bool App::runExportSheet(const std::string &path)
{
    SheetOptions options;
    PlayMode unused = PlayMode::Forward;
    exportRange(options.first, options.last, unused);
    options.layout = static_cast<SheetLayout>(export_.layout);
    options.columns = export_.columns;
    options.spacing = export_.spacing;
    options.scale = exportScale_;
    if (!spritesheet_export(editor_->sprite(), path, options)) {
        fileError_ = "Cannot write " + path + " (use " + image_write_formats() +
                     "; the sheet can be up to 16384 px)";
        return false;
    }
    return true;
}

bool App::runImportPalette(const std::string &path)
{
    std::optional<Palette> palette = palette_load(path);
    if (!palette) {
        fileError_ = "Cannot read " + path + " (a " + palette_formats() + " palette)";
        return false;
    }
    editor_->setPalette(std::move(*palette));
    return true;
}

bool App::runExportPalette(const std::string &path)
{
    if (!palette_save(editor_->sprite().palette(), path)) {
        fileError_ = "Cannot write " + path + " (use a " + palette_formats() + " file name)";
        return false;
    }
    return true;
}

void App::exportRange(int &first, int &last, PlayMode &direction) const
{
    const std::vector<Sprite::Tag> &tags = editor_->sprite().tags();
    first = 0;
    last = -1;
    direction = PlayMode::Forward;
    if (export_.tag >= 0 && export_.tag < static_cast<int>(tags.size())) {
        const Sprite::Tag &tag = tags[static_cast<size_t>(export_.tag)];
        first = tag.from;
        last = tag.to;
        direction = tag.direction;
    }
}

/* Whole-pixel enlargement, shown as the size it gives. */
void App::imageOptions()
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Scale");
    ImGui::SameLine();
    ImGui::InputInt("##export_scale", &exportScale_, 1, 2);
    exportScale_ = std::clamp(exportScale_, 1, IMAGE_EXPORT_MAX_SCALE);
    ImGui::SameLine();
    ImGui::TextDisabled("%d x %d px", editor_->sprite().width() * exportScale_,
                        editor_->sprite().height() * exportScale_);
}

/* The fields both animation exports share: which frames, and the enlargement. */
void App::animationOptions()
{
    const std::vector<Sprite::Tag> &tags = editor_->sprite().tags();
    if (export_.tag >= static_cast<int>(tags.size())) {
        export_.tag = -1;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Frames");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    std::string current =
        export_.tag < 0 ? "All frames" : tags[static_cast<size_t>(export_.tag)].name;
    if (ImGui::BeginCombo("##export_range", current.c_str())) {
        if (ImGui::Selectable("All frames", export_.tag < 0)) {
            export_.tag = -1;
        }
        for (int t = 0; t < static_cast<int>(tags.size()); t++) {
            ImGui::PushID(t);
            if (ImGui::Selectable(tags[static_cast<size_t>(t)].name.c_str(), t == export_.tag)) {
                export_.tag = t;
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
}

void App::gifOptions()
{
    animationOptions();
    ImGui::Checkbox("Loop forever", &export_.loop);
}

void App::sheetOptions()
{
    animationOptions();
    static const char *const LAYOUTS[] = {"Horizontal strip", "Vertical strip", "Grid"};
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Layout");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##sheet_layout", LAYOUTS[export_.layout])) {
        for (int i = 0; i < 3; i++) {
            if (ImGui::Selectable(LAYOUTS[i], i == export_.layout)) {
                export_.layout = i;
            }
        }
        ImGui::EndCombo();
    }
    if (export_.layout == static_cast<int>(SheetLayout::Grid)) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Columns");
        ImGui::SameLine();
        ImGui::InputInt("##sheet_columns", &export_.columns, 1, 2);
        export_.columns = std::clamp(export_.columns, 0, 256);
        ImGui::SameLine();
        ImGui::TextDisabled("0 = square-ish");
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Spacing");
    ImGui::SameLine();
    ImGui::InputInt("##sheet_spacing", &export_.spacing, 1, 4);
    export_.spacing = std::clamp(export_.spacing, 0, 256);
}

/* The last error and the overwrite warning for `path`, under the dialog's options. */
void App::fileDialogMessages(const std::string &path)
{
    if (!fileError_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", fileError_.c_str());
    }
    if (!overwritePath_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "%s already exists.",
                           file_name_of(path).c_str());
    }
}

/* Runs the dialog's action on `path` unless it is empty or needs an overwrite nobody allowed yet;
 * true when it succeeded and the dialog is done. */
bool App::runFileAction(const FileActionInfo &info, const std::string &path)
{
    if (path.empty() || (info.writes() && !overwriteAllowed(path))) {
        return false;
    }
    if (!(this->*info.run)(path)) {
        return false;
    }
    fileDone(path);
    return true;
}

/* Closes the dialog; a Save As that went through carries on with what waited for it. */
void App::closeFileDialog()
{
    bool saved = fileAction_ == FileAction::SaveAs && !dirty();
    fileAction_ = FileAction::None;
    ImGui::CloseCurrentPopup();
    std::function<void()> proceed = std::move(afterSave_);
    afterSave_ = nullptr;
    if (saved && proceed) {
        proceed();
    }
}

/* The "Path" field, focused when the dialog opens; true when Enter was pressed in it. */
bool App::pathField()
{
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Path");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(360.0f);
    return ImGui::InputText("##path", pathInput_, sizeof(pathInput_),
                            ImGuiInputTextFlags_EnterReturnsTrue);
}

void App::fileDialog()
{
    const FileActionInfo &info = fileInfo(fileAction_);
    const char *title = info.title;
    if (openFileDialog_) {
        ImGui::OpenPopup(title);
        openFileDialog_ = false;
    }
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    bool enter = pathField();
    /* A name typed without an extension gets the one the dialog writes. */
    const std::string typed = pathInput_;
    const std::string path = info.writes() ? with_default_extension(typed, info.extension) : typed;
    if (path != overwritePath_) {
        overwritePath_.clear(); /* the warning was for another name */
    }
    if (path != typed) {
        ImGui::TextDisabled("Will be written as %s", file_name_of(path).c_str());
    }
    if (info.options) {
        (this->*info.options)();
    }
    fileDialogMessages(path);
    ImGui::BeginDisabled(path.empty());
    bool confirm = ImGui::Button(overwritePath_.empty() ? "OK" : "Overwrite");
    ImGui::EndDisabled();
    bool done = (confirm || enter) && runFileAction(info, path);
    ImGui::SameLine();
    bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (done || cancel) {
        closeFileDialog();
    }
    ImGui::EndPopup();
}
