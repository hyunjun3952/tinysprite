/* App: opening, saving and exporting files, and the path dialog. */

#include "app.hpp"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "file_paths.hpp"
#include "icons.hpp"
#include "imgui.h"
#include "theme.hpp"
#include "tinysprite/io.hpp"

namespace
{

constexpr float PATH_FIELD_WIDTH = 360.0f; /* the field and the browse button after it */

} // namespace

/* One row per FileAction, in enum order: how the dialog is titled, the extension it adds to a name
 * typed without one ("" for the actions that read), what the toast says once it worked, the step
 * that does the work and the dialog fields it needs beyond the path. A new action is a row here and
 * a method, not a case in each of several switches. */
const App::FileActionInfo &App::fileInfo(FileAction action)
{
    static const FileActionInfo TABLE[] = {
        {"", "", "", nullptr, nullptr, ""},
        {"Open", "", "Opened ", &App::runOpen, nullptr,
         ".pxa .aseprite .ase .png .jpg .jpeg .bmp .tga .gif"},
        {"Save as", ".pxa", "Saved ", &App::runSaveAs, nullptr, ".pxa"},
        {"Export image", ".png", "Exported ", &App::runExportImage, &App::imageOptions,
         ".png .jpg .jpeg .bmp .tga"},
        {"Import palette", "", "Imported palette ", &App::runImportPalette, nullptr, ".gpl .hex"},
        {"Export palette", ".gpl", "Exported ", &App::runExportPalette, nullptr, ".gpl .hex"},
        {"Export GIF", ".gif", "Exported ", &App::runExportGif, &App::gifOptions, ".gif"},
        {"Export sprite sheet", ".png", "Exported ", &App::runExportSheet, &App::sheetOptions,
         ".png .jpg .jpeg .bmp .tga"},
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
    browseRoot_.clear(); /* the tree starts again from the folder of the path */
    browseCache_.clear();
    browseFoundKey_.clear();
    browseSearch_[0] = '\0';
    /* Start where the last file was, and offer the document's name for what is written. */
    std::string start;
    if (action == FileAction::SaveAs && !filePath().empty()) {
        start = filePath();
    } else if (fileInfo(action).writes()) {
        std::string name = filePath().empty() ? "sprite" : file_name_of(filePath());
        size_t dot = name.find_last_of('.');
        start =
            join_path(startDirectory(), (dot == std::string::npos ? name : name.substr(0, dot)) +
                                            fileInfo(action).extension);
    } else {
        start = join_path(startDirectory(), "");
    }
    std::snprintf(pathInput_, sizeof(pathInput_), "%s", start.c_str());
}

/* "Cannot write <path>" plus the reason the last file write gave, if it has one, else `hint` (what
 * the file name or size may have got wrong). */
std::string App::cannotWrite(const std::string &path, const std::string &hint)
{
    std::string reason = file_write_error();
    if (!reason.empty()) {
        return "Cannot write " + path + ": " + reason;
    }
    return "Cannot write " + path + (hint.empty() ? "" : " (" + hint + ")");
}

std::string App::existingOutput(const std::string &path) const
{
    std::error_code error;
    if (std::filesystem::exists(path_from_utf8(path), error)) {
        return path;
    }
    if (fileAction_ == FileAction::ExportSheet && export_.json) {
        const std::string json = with_replaced_extension(path, ".json");
        if (std::filesystem::exists(path_from_utf8(json), error)) {
            return json;
        }
    }
    return "";
}

bool App::overwriteAllowed(const std::string &path)
{
    const std::string existing = existingOutput(path);
    if (existing.empty() || (fileAction_ == FileAction::SaveAs && path == filePath()) ||
        overwritePath_ == path) {
        return true;
    }
    overwritePath_ = path;
    overwriteName_ = file_name_of(existing);
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
    if (filePath().empty()) {
        askForPath(FileAction::SaveAs);
    } else if (!pxa_save(editor_->sprite(), filePath())) {
        askForPath(FileAction::SaveAs);
        fileError_ = cannotWrite(filePath(), "");
        toast(fileError_, true);
    } else {
        editor_->markSaved();
        toast("Saved " + file_name_of(filePath()));
    }
}

bool App::openPath(const std::string &path)
{
    /* A file that is open already is just shown. */
    if (int open = findDocument(path); open >= 0) {
        activateDocument(open);
        return true;
    }
    /* A .pxa file starts with its magic number; anything else may be an image. */
    std::optional<Sprite> sprite = pxa_load(path);
    bool isPxa = sprite.has_value();
    if (!isPxa) {
        sprite = image_import(path);
    }
    if (!sprite) {
        fileError_ = "Cannot read " + path;
    } else if (!addDocument(std::move(*sprite), isPxa ? path : "", true)) {
        fileError_ = "Cannot open " + path + " right now";
    } else {
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
    if (int other = findDocument(path); other >= 0 && other != active_) {
        fileError_ = file_name_of(path) + " is open in another tab; close that tab first";
        return false;
    }
    if (!pxa_save(editor_->sprite(), path)) {
        fileError_ = cannotWrite(path, "");
        return false;
    }
    filePath() = path;
    recent_add(recent_, path, RECENT_MAX);
    editor_->markSaved();
    return true;
}

bool App::runExportImage(const std::string &path)
{
    if (!image_export(editor_->sprite(), editor_->frame(), path, exportScale_)) {
        fileError_ =
            cannotWrite(path, "use " + image_write_formats() + "; the image can be up to 16384 px");
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
        fileError_ = cannotWrite(path, "use a .gif file name; up to 16384 px");
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
    const bool written =
        export_.json ? spritesheet_export_with_json(editor_->sprite(), path,
                                                    with_replaced_extension(path, ".json"), options)
                     : spritesheet_export(editor_->sprite(), path, options);
    if (!written) {
        fileError_ =
            cannotWrite(path, "use " + image_write_formats() + "; the sheet can be up to 16384 px");
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
        fileError_ = cannotWrite(path, "use a " + palette_formats() + " file name");
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
    theme::input_int("##export_scale", &exportScale_, 1, 2, "\xC3\x97");
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
    ImGui::SetNextItemWidth(theme::px(200.0f));
    std::string current =
        export_.tag < 0 ? "All frames" : tags[static_cast<size_t>(export_.tag)].name;
    if (theme::begin_combo("##export_range", current.c_str())) {
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
    theme::input_int("##export_scale", &exportScale_, 1, 2, "\xC3\x97");
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
    static const char *const LAYOUTS[] = {"Horizontal strip", "Vertical strip", "Grid",
                                          "Packed (trimmed)"};
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Layout");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(theme::px(200.0f));
    if (theme::begin_combo("##sheet_layout", LAYOUTS[export_.layout])) {
        for (int i = 0; i < static_cast<int>(std::size(LAYOUTS)); i++) {
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
        theme::input_int("##sheet_columns", &export_.columns, 1, 2, "");
        export_.columns = std::clamp(export_.columns, 0, 256);
        ImGui::SameLine();
        ImGui::TextDisabled("0 = square-ish");
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Spacing");
    ImGui::SameLine();
    theme::input_int("##sheet_spacing", &export_.spacing, 1, 4, " px");
    export_.spacing = std::clamp(export_.spacing, 0, 256);
    ImGui::Checkbox("Also write a .json file", &export_.json);
    theme::tip("The frames (and, for a packed sheet, where each piece was trimmed from), their "
               "durations, the tags and the slices, for a game to load next to the image.");
}

/* The last error and the overwrite warning for `path`, under the dialog's options. */
void App::fileDialogMessages()
{
    if (!fileError_.empty()) {
        ImGui::TextUnformatted(fileError_.c_str());
    }
    if (!overwritePath_.empty()) { /* overwriteName_ is the image or, for a sheet, its .json */
        ImGui::TextUnformatted((overwriteName_ + " already exists.").c_str());
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
    const float button = ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(PATH_FIELD_WIDTH - button - ImGui::GetStyle().ItemSpacing.x);
    const bool enter = ImGui::InputText("##path", pathInput_, sizeof(pathInput_),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    /* on = a slightly lighter fill, not the accent: the dialog's own OK is the loud thing here */
    ImGui::PushStyleColor(ImGuiCol_Button,
                          theme::rgb(browserOpen_ ? theme::PRESSED : theme::CANVAS_SOFT));
    if (icons::button(icons::Icon::Folder, "##browse", "Browse folders and files",
                      ImVec2(button, button))) {
        browserOpen_ = !browserOpen_;
    }
    ImGui::PopStyleColor();
    return enter;
}

std::string App::startDirectory() const
{
    if (!lastDir_.empty()) {
        return lastDir_;
    }
    std::error_code ignored;
    return path_to_utf8(std::filesystem::current_path(ignored));
}

/* What the dialog would write or read: "~" is the home folder, and a write gets its extension. */
std::string App::dialogPath(const FileActionInfo &info, const std::string &typed) const
{
    std::string path = expand_home(typed);
    if (!info.writes()) {
        return path;
    }
    return fileAction_ == FileAction::SaveAs ? with_required_extension(path, info.extension)
                                             : with_default_extension(path, info.extension);
}

/* The path the dialog will write: a name typed without an extension gets the one the dialog
 * writes, and the difference is shown. */
std::string App::dialogTarget(const FileActionInfo &info)
{
    const std::string typed = expand_home(pathInput_);
    std::string path = dialogPath(info, pathInput_);
    if (path != overwritePath_) {
        overwritePath_.clear(); /* the warning was for another name */
    }
    if (path != typed) {
        ImGui::TextDisabled("Will be written as %s", file_name_of(path).c_str());
    }
    return path;
}

void App::fileDialog()
{
    const FileActionInfo &info = fileInfo(fileAction_);
    const char *title = info.title;
    if (openFileDialog_) {
        ImGui::OpenPopup(title);
        openFileDialog_ = false;
    }
    if (!theme::begin_modal(title)) {
        return;
    }
    bool enter = pathField();
    if (browserOpen_) {
        enter = fileBrowser(info) || enter;
    }
    const std::string path = dialogTarget(info);
    if (info.options) {
        (this->*info.options)();
    }
    fileDialogMessages();
    /* overwriting a file is the one thing here that destroys something: its button is red */
    const bool overwriting = !overwritePath_.empty();
    const int clicked = theme::dialog_buttons({"Cancel", overwriting ? "Overwrite" : "OK"}, 1,
                                              overwriting, path.empty() ? 2u : 0u);
    bool done = (clicked == 1 || enter) && runFileAction(info, path);
    bool cancel = clicked == 0 || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (done || cancel) {
        closeFileDialog();
    }
    theme::end_modal();
}
