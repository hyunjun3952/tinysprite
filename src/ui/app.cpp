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
constexpr int BRUSH_STEP_BIG = 5; /* the brush size step with Shift held */

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
    window_ = SDL_CreateWindow("tinysprite", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (window_) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
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
    canvas_ = std::make_unique<Canvas>(*editor_);
    toolsPanel_ = std::make_unique<ToolsPanel>(*editor_);
    layersPanel_ = std::make_unique<LayersPanel>(*editor_);
    if (!options.file.empty() && !openPath(options.file)) {
        std::fprintf(stderr, "%s\n", fileError_.c_str());
        return false;
    }
    return true;
}

bool App::dirty() const
{
    return !editor_->history().atSavePoint();
}

/* "name.pxa" or "untitled", with a trailing "*" while there are unsaved changes. */
void App::updateTitle()
{
    std::string name =
        filePath_.empty() ? "untitled" : filePath_.substr(filePath_.find_last_of('/') + 1);
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
    guardUnsaved([this, path] {
        if (!openPath(path)) {
            std::string error = fileError_;
            askForPath(FileAction::Open);
            fileError_ = error;
            std::snprintf(pathInput_, sizeof(pathInput_), "%s", path.c_str());
        }
    });
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
        if (ImGui::MenuItem("Open...", "Ctrl+O", false, idle)) {
            guardUnsaved([this] { askForPath(FileAction::Open); });
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            save();
        }
        if (ImGui::MenuItem("Save as...")) {
            askForPath(FileAction::SaveAs);
        }
        if (ImGui::MenuItem("Export image...")) {
            askForPath(FileAction::ExportImage);
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
        if (ImGui::MenuItem("Zoom in", "Wheel up")) {
            editor_->zoomStep(1);
        }
        if (ImGui::MenuItem("Zoom out", "Wheel down")) {
            editor_->zoomStep(-1);
        }
        if (ImGui::MenuItem("Fit", "F")) {
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

/* Keys that act without a modifier: tool letters, X (swap colors), + / - and [ ] (brush size, 5 at
 * a time with Shift) and F (fit). */
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
    /* + / - (and [ / ]) change the brush size by 1, or by 5 with Shift held; the wheel zooms. */
    int step = ImGui::GetIO().KeyShift ? BRUSH_STEP_BIG : 1;
    if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd) ||
        ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
        editor_->changeBrushSize(step);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract) ||
        ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
        editor_->changeBrushSize(-step);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        editor_->view().needs_fit = true;
    }
}

void App::handleShortcuts()
{
    const ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput || layersPanel_->dialogOpen()) {
        editor_->tempPick(false);
        return;
    }
    /* Holding Alt turns the active tool into the eyedropper until it is released. */
    editor_->tempPick(io.KeyAlt);
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
    if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        guardUnsaved([this] { askForPath(FileAction::Open); });
    } else if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        save();
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
        {"Alt (hold)", "Temporary eyedropper"},
        {"X", "Swap foreground / background colors"},
        {"+  -  or  [  ]", "Larger / smaller brush: 1 px, or 5 px with Shift"},
        {"Shift + click", "Pencil / eraser: straight line from the last point"},
        {"Left drag", "Draw with the active tool (inside the selection, if there is one)"},
        {"Wheel", "Zoom at the cursor"},
        {"Right / middle drag, Space + drag", "Pan"},
        {"Mouse wheel, F", "Zoom at the cursor, fit the sprite"},
        {"Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z", "Undo, redo"},
        {"Ctrl+A, Delete, Esc", "Select all, clear the selection's pixels, cancel / deselect"},
        {"Click / Ctrl+click / Shift+click on layers",
         "Select one layer, add or remove one, select a range"},
        {"Delete (layer list focused)", "Delete the selected layers, after asking"},
        {"Ctrl+C, Ctrl+X, Ctrl+V", "Copy, cut, paste"},
        {"Ctrl+O, Ctrl+S", "Open, save"},
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
        editor_->history().markSaved();
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
        editor_->history().markSaved();
        return true;
    case FileAction::ExportImage:
        if (!image_export(editor_->sprite(), editor_->frame(), path)) {
            fileError_ = "Cannot write " + path;
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
    static const char *const TITLES[] = {"", "Open", "Save as", "Export image"};
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
    bool enter = ImGui::InputText("Path", pathInput_, sizeof(pathInput_),
                                  ImGuiInputTextFlags_EnterReturnsTrue);
    const std::string path = pathInput_;
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
        helpWindow();
        handleShortcuts();
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
}
