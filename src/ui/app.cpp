#include "app.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

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

void App::setUiScale(float scale)
{
    float before = theme::scale();
    theme::setScale(scale);
    theme::apply();
    panels_.rescale(theme::scale() / before);
}

void App::toast(const std::string &text, bool error)
{
    toasts_.push(text, error, ImGui::GetTime());
}

void App::loadSettings(const SettingsMap &settings)
{
    editor_->load(settings);
    panels_.rescale(theme::scale()); /* the default widths, at this scale */
    panels_.load(settings);
    auto dir = settings.find("file.dir");
    if (dir != settings.end()) {
        lastDir_ = dir->second;
    }
    recent_ = settings_get_recent(settings, RECENT_MAX);
    settings_get_int(settings, "export.scale", 1, IMAGE_EXPORT_MAX_SCALE, exportScale_);
    settings_get_int(settings, "new.width", 1, CEL_MAX_DIMENSION, newWidth_);
    settings_get_int(settings, "new.height", 1, CEL_MAX_DIMENSION, newHeight_);
}

/* The scale comes from --scale, else the saved one, else what the display's DPI suggests. */
static float initialScale(const AppOptions &options, const SettingsMap &settings)
{
    if (options.uiScale > 0.0f) {
        return options.uiScale;
    }
    float scale = 1.0f;
    if (settings_get_float(settings, "ui.scale", theme::SCALE_MIN, theme::SCALE_MAX, scale)) {
        return scale;
    }
    float dpi = 0.0f;
    return SDL_GetDisplayDPI(0, &dpi, nullptr, nullptr) == 0 ? theme::scaleForDpi(dpi) : 1.0f;
}

/* The saved window size; a default, scaled with the UI, when none was saved. */
static void initialWindowSize(const SettingsMap &settings, int &width, int &height)
{
    width = WINDOW_WIDTH;
    height = WINDOW_HEIGHT;
    settings_get_int(settings, "window.width", 320, 16384, width);
    settings_get_int(settings, "window.height", 240, 16384, height);
    if (!settings.count("window.width")) {
        width = static_cast<int>(theme::px(static_cast<float>(width)));
        height = static_cast<int>(theme::px(static_cast<float>(height)));
    }
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
    theme::setScale(initialScale(options, settings));
    int windowWidth = 0;
    int windowHeight = 0;
    initialWindowSize(settings, windowWidth, windowHeight);
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
    settings_set_float(settings, "ui.scale", theme::scale());
    settings["file.dir"] = lastDir_;
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

/* Reads the rendered frame back and writes it as an image chosen by the extension of `path` (a
 * development aid for checking the look without a display). */
bool App::saveScreenshot(const std::string &path) const
{
    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(renderer_, &width, &height) != 0) {
        return false;
    }
    std::vector<Uint32> argb(static_cast<size_t>(width) * static_cast<size_t>(height));
    if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, argb.data(),
                             width * static_cast<int>(sizeof(Uint32))) != 0) {
        return false;
    }
    std::vector<Rgba> pixels;
    pixels.reserve(argb.size());
    for (Uint32 value : argb) {
        pixels.push_back({static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 8),
                          static_cast<uint8_t>(value), 255});
    }
    return image_write_rgba(path, width, height, pixels);
}

/* Feeds the window's events to ImGui and handles the ones the app owns. */
void App::pollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) {
            guardUnsaved([this] { running_ = false; });
        } else if (event.type == SDL_DROPFILE) {
            dropFile(event.drop.file);
            SDL_free(event.drop.file);
        }
    }
}

/* Lays out and builds every panel and dialog of one UI frame. */
void App::buildUi()
{
    menuBar();
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    float left = 0.0f;
    float right = 0.0f;
    panels_.layout(left, right);
    float status = editor_->overlays().status ? status_bar_height() : 0.0f;
    float body = viewport->WorkSize.y - status; /* above the status bar */
    toolsPanel_->show(panels_, body);
    layersWindow(body);
    Uint32 now = SDL_GetTicks();
    editor_->tick(static_cast<int>(now - lastTick_));
    lastTick_ = now;
    float timeline = editor_->overlays().timeline ? TimelinePanel::height() : 0.0f;
    if (timeline > 0.0f) {
        timelinePanel_->show(viewport->WorkPos.x + left, viewport->WorkPos.y + body - timeline,
                             viewport->WorkSize.x - left - right);
    }
    canvas_->show(viewport->WorkPos.x + left, viewport->WorkPos.y,
                  viewport->WorkSize.x - left - right, body - timeline);
    if (status > 0.0f) {
        status_bar_show(*editor_, viewport->WorkPos.x, viewport->WorkPos.y + body,
                        viewport->WorkSize.x);
    }
    toasts_.draw(ImGui::GetTime(), status + theme::px(16.0f));
    fileDialog();
    confirmDialog();
    newDialog();
    recoverDialog();
    helpWindow();
    handleShortcuts();
    autosave();
    updateTitle();
}

void App::run(const AppOptions &options)
{
    int frames = 0;
    while (running_) {
        pollEvents();
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        buildUi();

        SDL_SetRenderDrawColor(renderer_, (theme::WORKSPACE >> 16) & 0xff,
                               (theme::WORKSPACE >> 8) & 0xff, theme::WORKSPACE & 0xff, 255);
        SDL_RenderClear(renderer_);
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
        bool lastFrame = options.maxFrames > 0 && frames + 1 >= options.maxFrames;
        if (lastFrame && !options.screenshot.empty() && !saveScreenshot(options.screenshot)) {
            std::fprintf(stderr, "cannot save %s\n", options.screenshot.c_str());
        }
        SDL_RenderPresent(renderer_);

        frames++;
        running_ = running_ && !lastFrame;
    }
    shutdown();
}
