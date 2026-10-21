#include "app/app.h"

#include <cstdio>

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "icons/icons.h"
#include "imgui.h"

namespace
{

constexpr int WINDOW_WIDTH = 1100;
constexpr int WINDOW_HEIGHT = 720;

} // namespace

App::~App()
{
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
    ImGui::StyleColorsDark();
    /* Unchecked boxes and sliders must stand out from the dark panel background. */
    ImGuiStyle &style = ImGui::GetStyle();
    style.FrameBorderSize = 1.0f;
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.24f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.42f, 0.43f, 0.48f, 0.8f);
    if (ImGui_ImplSDL2_InitForSDLRenderer(window_, renderer_)) {
        if (ImGui_ImplSDLRenderer2_Init(renderer_)) {
            backends_ = true;
        } else {
            ImGui_ImplSDL2_Shutdown();
        }
    }
    if (backends_ && !icons::init()) {
        std::fprintf(stderr, "icon font failed to load; buttons will have no icons\n");
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
    return true;
}

void App::layersWindow(float height)
{
    if (panels_.begin(PanelId::Layers, height)) {
        layersPanel_->show();
    }
    panels_.end();
}

void App::menuBar()
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        /* Disabled until the .pxa reader / writer exists (milestone 7, src/io is still empty). */
        ImGui::MenuItem("Open...", "Ctrl+O", false, false);
        ImGui::MenuItem("Save", "Ctrl+S", false, false);
        ImGui::MenuItem("Export...", nullptr, false, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) {
            running_ = false;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        bool idle = !editor_->stroking();
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false,
                            idle && history_can_undo(editor_->history()))) {
            editor_->undo();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false,
                            idle && history_can_redo(editor_->history()))) {
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
        if (ImGui::MenuItem("Zoom in", "+")) {
            editor_->zoomStep(1);
        }
        if (ImGui::MenuItem("Zoom out", "-")) {
            editor_->zoomStep(-1);
        }
        if (ImGui::MenuItem("Fit", "F")) {
            editor_->view().needs_fit = true;
        }
        bool grid = editor_->showGrid();
        if (ImGui::MenuItem("Grid", nullptr, &grid)) {
            editor_->setShowGrid(grid);
        }
        ImGui::EndMenu();
    }
    panels_.windowMenu();
    ImGui::EndMainMenuBar();
}

/* Keys that act without a modifier: tool letters, X (swap colors), [ ] (brush size), F (fit) and
 * +/- (zoom). */
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
    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket) && editor_->options().brush_size > 1) {
        editor_->options().brush_size--;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket) &&
        editor_->options().brush_size < PENCIL_BRUSH_MAX) {
        editor_->options().brush_size++;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        editor_->view().needs_fit = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
        editor_->zoomStep(1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
        editor_->zoomStep(-1);
    }
}

void App::handleShortcuts()
{
    const ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput) {
        editor_->tempPick(false);
        return;
    }
    /* Holding Alt turns the active tool into the eyedropper until it is released. */
    editor_->tempPick(io.KeyAlt);
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
        editor_->deleteSelection();
    }
    if (!io.KeyCtrl) {
        if (!io.KeyAlt) {
            plainShortcuts();
        }
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
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

void App::run(const AppOptions &options)
{
    int frames = 0;
    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running_ = false;
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
        handleShortcuts();

        SDL_SetRenderDrawColor(renderer_, 30, 30, 32, 255);
        SDL_RenderClear(renderer_);
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
        SDL_RenderPresent(renderer_);

        if (options.maxFrames > 0 && ++frames >= options.maxFrames) {
            running_ = false;
        }
    }
}
