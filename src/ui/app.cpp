#include "app.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <optional>
#include <utility>

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "imgui.h"
#include "theme.h"
#include "tinysprite/io.h"

namespace
{

constexpr int WINDOW_WIDTH = 1100;
constexpr int WINDOW_HEIGHT = 720;
constexpr Uint32 AUTOSAVE_INTERVAL_MS = 30000;
constexpr size_t RECENT_MAX = 10;

} // namespace

App::~App()
{
    /* Everything that owns textures goes before the renderer they belong to. */
    layersPanel_.reset();
    toolsPanel_.reset();
    canvas_.reset();
    editor_.reset();
    if (backends_) {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
    }
    if (imguiContext_) {
        ImGui::DestroyContext();
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}

bool App::init(const AppOptions &options)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    /* Smoke runs (--frames) neither read nor write the user's settings and autosave. */
    persist_ = options.maxFrames == 0 || !options.configDir.empty();
    configDir_ = !options.configDir.empty() ? options.configDir : persist_ ? config_dir() : "";
    SettingsMap settings = settings_load(configDir_);
    int windowWidth = WINDOW_WIDTH;
    int windowHeight = WINDOW_HEIGHT;
    settings_get_int(settings, "window.width", 320, 16384, windowWidth);
    settings_get_int(settings, "window.height", 240, 16384, windowHeight);
    window_ = SDL_CreateWindow("tinysprite", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               windowWidth, windowHeight, SDL_WINDOW_RESIZABLE);
    if (window_) {
        renderer_ =
            SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer_) {
            renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        }
    }
    if (!renderer_) {
        std::fprintf(stderr, "SDL window/renderer: %s\n", SDL_GetError());
        return false;
    }

    ImGui::CreateContext();
    imguiContext_ = true;
    ImGui::GetIO().IniFilename = nullptr;
    theme::apply();
    if (ImGui_ImplSDL2_InitForSDLRenderer(window_, renderer_)) {
        if (ImGui_ImplSDLRenderer2_Init(renderer_)) {
            backends_ = true;
        } else {
            ImGui_ImplSDL2_Shutdown();
        }
    }
    if (backends_) {
        editor_ = Editor::create(renderer_, options.width, options.height);
    }
    if (!editor_) {
        std::fprintf(stderr, "failed to initialise the editor\n");
        return false;
    }
    editor_->load(settings);
    panels_.load(settings);
    recent_ = settings_get_recent(settings, RECENT_MAX);
    settings_get_int(settings, "export.scale", 1, IMAGE_EXPORT_MAX_SCALE, exportScale_);
    settings_get_int(settings, "new.width", 1, CEL_MAX_DIMENSION, newWidth_);
    settings_get_int(settings, "new.height", 1, CEL_MAX_DIMENSION, newHeight_);
    canvas_ = std::make_unique<Canvas>(*editor_);
    toolsPanel_ = std::make_unique<ToolsPanel>(*editor_);
    layersPanel_ = std::make_unique<LayersPanel>(*editor_);
    if (!options.file.empty() && !openPath(options.file)) {
        std::fprintf(stderr, "%s\n", fileError_.c_str());
        return false;
    }
    if (persist_) {
        recovered_ = recovery_load(configDir_);
        openRecover_ = recovered_.has_value();
    }
    lastAutosave_ = SDL_GetTicks();
    return true;
}

bool App::dirty() const
{
    return !editor_->history().atSavePoint() || editor_->modifiedOutsideHistory();
}

/* "name.pxa" or "untitled", with a trailing "*" while there are unsaved changes. */
void App::updateTitle()
{
    std::string name =
        filePath_.empty() ? "untitled" : filePath_.substr(filePath_.find_last_of("/\\") + 1);
    std::string title = (dirty() ? name + "*" : name) + " - tinysprite";
    if (title != title_) {
        title_ = title;
        SDL_SetWindowTitle(window_, title_.c_str());
    }
}

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

/* While the document has unsaved changes, a copy goes to the config directory every so often; a
 * document that is clean again no longer needs one. */
void App::autosave()
{
    if (!persist_ || recovered_) {
        return;
    }
    if (!dirty()) {
        if (hasAutosave_) {
            recovery_clear(configDir_);
            hasAutosave_ = false;
        }
        return;
    }
    Uint32 now = SDL_GetTicks();
    if (editor_->stroking() || now - lastAutosave_ < AUTOSAVE_INTERVAL_MS) {
        return;
    }
    lastAutosave_ = now;
    hasAutosave_ = recovery_save(configDir_, editor_->sprite(), filePath_) || hasAutosave_;
}

void App::shutdown()
{
    if (!persist_) {
        return;
    }
    SettingsMap settings = settings_load(configDir_); /* keep keys this version does not know */
    editor_->store(settings);
    panels_.store(settings);
    settings_set_recent(settings, recent_);
    settings_set_int(settings, "export.scale", exportScale_);
    settings_set_int(settings, "new.width", newWidth_);
    settings_set_int(settings, "new.height", newHeight_);
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window_, &width, &height);
    settings_set_int(settings, "window.width", width);
    settings_set_int(settings, "window.height", height);
    settings_save(configDir_, settings);
    if (!recovered_) {
        recovery_clear(configDir_);
    }
}

void App::layersWindow(float height)
{
    if (panels_.begin(PanelId::Layers, height)) {
        layersPanel_->show(panels_);
    }
    panels_.end();
}

void App::menuBar()
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        bool idle = !editor_->stroking();
        if (ImGui::MenuItem("New...", "Ctrl+N", false, idle)) {
            guardUnsaved([this] { openNew_ = true; });
        }
        if (ImGui::MenuItem("Open...", "Ctrl+O", false, idle)) {
            guardUnsaved([this] { askForPath(FileAction::Open); });
        }
        if (ImGui::BeginMenu("Open recent", idle && !recent_.empty())) {
            std::string chosen;
            for (size_t i = 0; i < recent_.size(); i++) {
                ImGui::PushID(static_cast<int>(i));
                std::string name = recent_[i].substr(recent_[i].find_last_of("/\\") + 1);
                if (ImGui::MenuItem(name.c_str())) {
                    chosen = recent_[i];
                }
                theme::tip(recent_[i].c_str());
                ImGui::PopID();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear list")) {
                recent_.clear();
            }
            ImGui::EndMenu();
            if (!chosen.empty()) {
                guardUnsaved([this, chosen] { openOrAsk(chosen); });
            }
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            save();
        }
        if (ImGui::MenuItem("Save as...", "Ctrl+Shift+S")) {
            askForPath(FileAction::SaveAs);
        }
        if (ImGui::MenuItem("Export image...", "Ctrl+E")) {
            askForPath(FileAction::ExportImage);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Import palette...", nullptr, false, idle)) {
            askForPath(FileAction::ImportPalette);
        }
        if (ImGui::MenuItem("Export palette...")) {
            askForPath(FileAction::ExportPalette);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) {
            guardUnsaved([this] { running_ = false; });
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        bool idle = !editor_->stroking();
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, idle && editor_->history().canUndo())) {
            editor_->undo();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, idle && editor_->history().canRedo())) {
            editor_->redo();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Cut", "Ctrl+X", false, idle && editor_->hasSelection())) {
            editor_->cutSelection();
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, idle && editor_->hasSelection())) {
            editor_->copySelection();
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, idle)) {
            editor_->paste();
        }
        if (ImGui::MenuItem("Select all", "Ctrl+A", false, idle)) {
            editor_->selectAll();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Zoom in", "Ctrl++")) {
            editor_->zoomStep(1);
        }
        if (ImGui::MenuItem("Zoom out", "Ctrl+-")) {
            editor_->zoomStep(-1);
        }
        if (ImGui::MenuItem("Fit", "F, Ctrl+0")) {
            editor_->view().needs_fit = true;
        }
        ImGui::Separator();
        bool grid = editor_->showGrid();
        if (ImGui::MenuItem("Pixel grid", nullptr, &grid)) {
            editor_->setShowGrid(grid);
        }
        ImGui::MenuItem("Cursor outline", nullptr, &editor_->overlays().cursor);
        theme::tip("Outline the cel under the mouse, and the footprint of a big brush.");
        ImGui::MenuItem("Mirror axes", nullptr, &editor_->overlays().mirror_axes);
        theme::tip("Draw the center lines that Mirror X / Y reflect across.");
        ImGui::MenuItem("Status line", nullptr, &editor_->overlays().status);
        theme::tip("Show the size, zoom, active layer and hovered cel in the canvas corner.");
        ImGui::EndMenu();
    }
    panels_.windowMenu();
    if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("Keyboard shortcuts", "F1", &showHelp_);
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

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

void App::handleShortcuts()
{
    const ImGuiIO &io = ImGui::GetIO();
    /* The file / unsaved-changes dialogs are modal: nothing behind them may react. */
    bool modal = fileAction_ != FileAction::None || ImGui::IsPopupOpen("Unsaved changes") ||
                 openConfirm_ || openNew_ || openRecover_ || newShown_ || recoverShown_;
    if (io.WantTextInput || layersPanel_->dialogOpen() || toolsPanel_->dialogOpen() || modal) {
        editor_->tempPick(false);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false) ||
        (io.KeyShift && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Slash, false))) {
        showHelp_ = !showHelp_;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (editor_->stroking()) {
            editor_->cancelDrag();
        } else {
            editor_->clearSelection();
        }
    }
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
    if (!io.KeyCtrl) {
        if (!io.KeyAlt) {
            plainShortcuts();
        }
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        guardUnsaved([this] { openNew_ = true; });
    } else if (ImGui::IsKeyPressed(ImGuiKey_0, false) ||
               ImGui::IsKeyPressed(ImGuiKey_Keypad0, false)) {
        editor_->view().needs_fit = true;
    } else if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
        editor_->zoomStep(1);
    } else if (ImGui::IsKeyPressed(ImGuiKey_Minus) ||
               ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
        editor_->zoomStep(-1);
    } else if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        guardUnsaved([this] { askForPath(FileAction::Open); });
    } else if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        if (io.KeyShift) {
            askForPath(FileAction::SaveAs);
        } else {
            save();
        }
    } else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        askForPath(FileAction::ExportImage);
    } else if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (io.KeyShift) {
            editor_->redo();
        } else {
            editor_->undo();
        }
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
        editor_->redo();
    } else if (ImGui::IsKeyPressed(ImGuiKey_A, false)) {
        editor_->selectAll();
    } else if (ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        editor_->copySelection();
    } else if (ImGui::IsKeyPressed(ImGuiKey_X, false)) {
        editor_->cutSelection();
    } else if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
        editor_->paste();
    }
}

/* Every shortcut in one place, since the tooltips only mention the tool letters. */
void App::helpWindow()
{
    if (!showHelp_) {
        return;
    }
    static const char *const ROWS[][2] = {
        {"B E G I L R O M", "Pencil, eraser, bucket, eyedropper, line, rectangle, ellipse, select"},
        {"T D H S C", "Triangle, diamond, hexagon, star, cross (shapes)"},
        {"Hold middle button", "Eyedropper while held (drag instead to pan)"},
        {"X", "Swap foreground / background colors"},
        {"+  -  or  [  ]", "Larger / smaller brush by 1 px"},
        {"Shift + click", "Pencil / eraser: straight line from the last point"},
        {"Left drag", "Draw with the active tool (inside the selection, if there is one)"},
        {"Wheel", "Zoom at the cursor"},
        {"Ctrl + / Ctrl - / Ctrl+0", "Zoom in / out / fit the sprite"},
        {"Shift (while dragging a shape)", "Square box or circle; a line snaps to 45 degrees"},
        {"Right / middle drag, Space + drag", "Pan"},
        {"Mouse wheel, F", "Zoom at the cursor, fit the sprite"},
        {"Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z", "Undo, redo"},
        {"Ctrl+A, Delete, Esc", "Select all, clear the selection's pixels, cancel / deselect"},
        {"Click / Ctrl+click / Shift+click on layers",
         "Select one layer, add or remove one, select a range"},
        {"Delete (layer list focused)", "Delete the selected layers, after asking"},
        {"Ctrl+C, Ctrl+X, Ctrl+V", "Copy, cut, paste"},
        {"Ctrl+N, Ctrl+O", "New, open"},
        {"Ctrl+S, Ctrl+Shift+S, Ctrl+E", "Save, save as, export image"},
        {"F1", "This window"},
    };
    ImGui::SetNextWindowSize(ImVec2(620, 0), ImGuiCond_Appearing);
    if (ImGui::Begin("Keyboard shortcuts", &showHelp_, ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::BeginTable("shortcuts", 2,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            for (const auto &row : ROWS) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row[0]);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row[1]);
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

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

/* Reads the rendered frame back and writes it as a BMP (a development aid for checking the look
 * without a display). */
bool App::saveScreenshot(const std::string &path) const
{
    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(renderer_, &width, &height) != 0) {
        return false;
    }
    SDL_Surface *surface =
        SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        return false;
    }
    bool ok = SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels,
                                   surface->pitch) == 0 &&
              SDL_SaveBMP(surface, path.c_str()) == 0;
    SDL_FreeSurface(surface);
    return ok;
}

void App::run(const AppOptions &options)
{
    int frames = 0;
    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                guardUnsaved([this] { running_ = false; });
            } else if (event.type == SDL_DROPFILE) {
                dropFile(event.drop.file);
            }
            if (event.type == SDL_DROPFILE) {
                SDL_free(event.drop.file);
            }
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        menuBar();
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        float left = 0.0f;
        float right = 0.0f;
        panels_.layout(left, right);
        toolsPanel_->show(panels_, viewport->WorkSize.y);
        layersWindow(viewport->WorkSize.y);
        canvas_->show(viewport->WorkPos.x + left, viewport->WorkPos.y,
                      viewport->WorkSize.x - left - right, viewport->WorkSize.y);
        fileDialog();
        confirmDialog();
        newDialog();
        recoverDialog();
        helpWindow();
        handleShortcuts();
        autosave();
        updateTitle();

        SDL_SetRenderDrawColor(renderer_, (theme::WORKSPACE >> 16) & 0xff,
                               (theme::WORKSPACE >> 8) & 0xff, theme::WORKSPACE & 0xff, 255);
        SDL_RenderClear(renderer_);
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
        if (!options.screenshot.empty() && options.maxFrames > 0 &&
            frames + 1 >= options.maxFrames && !saveScreenshot(options.screenshot)) {
            std::fprintf(stderr, "cannot save %s\n", options.screenshot.c_str());
        }
        SDL_RenderPresent(renderer_);

        if (options.maxFrames > 0 && ++frames >= options.maxFrames) {
            running_ = false;
        }
    }
    shutdown();
}
