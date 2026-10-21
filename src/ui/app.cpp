#include "app.h"

#include <cstdio>
#include <memory>
#include <string>

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

bool App::createWindow(int width, int height)
{
    window_ = SDL_CreateWindow("tinysprite", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                               height, SDL_WINDOW_RESIZABLE);
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
    return true;
}

bool App::initImGui()
{
    ImGui::CreateContext();
    imguiContext_ = true;
    ImGui::GetIO().IniFilename = nullptr;
    theme::apply();
    if (!ImGui_ImplSDL2_InitForSDLRenderer(window_, renderer_)) {
        return false;
    }
    if (!ImGui_ImplSDLRenderer2_Init(renderer_)) {
        ImGui_ImplSDL2_Shutdown();
        return false;
    }
    backends_ = true;
    return true;
}

void App::loadSettings(const SettingsMap &settings)
{
    editor_->load(settings);
    panels_.load(settings);
    recent_ = settings_get_recent(settings, RECENT_MAX);
    settings_get_int(settings, "export.scale", 1, IMAGE_EXPORT_MAX_SCALE, exportScale_);
    settings_get_int(settings, "new.width", 1, CEL_MAX_DIMENSION, newWidth_);
    settings_get_int(settings, "new.height", 1, CEL_MAX_DIMENSION, newHeight_);
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
    if (!createWindow(windowWidth, windowHeight)) {
        return false;
    }
    if (initImGui()) {
        editor_ = Editor::create(renderer_, options.width, options.height);
    }
    if (!editor_) {
        std::fprintf(stderr, "failed to initialise the editor\n");
        return false;
    }
    loadSettings(settings);
    canvas_ = std::make_unique<Canvas>(*editor_);
    toolsPanel_ = std::make_unique<ToolsPanel>(*editor_);
    layersPanel_ = std::make_unique<LayersPanel>(*editor_);
    timelinePanel_ = std::make_unique<TimelinePanel>(*editor_);
    lastTick_ = SDL_GetTicks();
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
        Uint32 now = SDL_GetTicks();
        editor_->tick(static_cast<int>(now - lastTick_));
        lastTick_ = now;
        float timeline = editor_->overlays().timeline ? TimelinePanel::HEIGHT : 0.0f;
        if (timeline > 0.0f) {
            timelinePanel_->show(viewport->WorkPos.x + left,
                                 viewport->WorkPos.y + viewport->WorkSize.y - timeline,
                                 viewport->WorkSize.x - left - right);
        }
        canvas_->show(viewport->WorkPos.x + left, viewport->WorkPos.y,
                      viewport->WorkSize.x - left - right, viewport->WorkSize.y - timeline);
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
