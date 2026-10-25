#include "app.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "icons.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_internal.h"
#include "theme.hpp"
#include "tinysprite/io.hpp"

namespace
{

constexpr int WINDOW_WIDTH = 1100;
constexpr int WINDOW_HEIGHT = 720;
constexpr Uint64 AUTOSAVE_INTERVAL_MS = 30000;
constexpr float OPTIONS_BAR_HEIGHT = 40.0f; /* at scale 1 */
constexpr float SASH_THICKNESS = 8.0f;      /* the grab area on the timeline's edge */
constexpr float TIMELINE_MAX_SHARE = 0.7f;  /* of the canvas area, the timeline's largest height */

} // namespace

App::~App()
{
    /* Everything that owns textures goes before the renderer they belong to. */
    layersPanel_.reset();
    tilemapWindow_.release();
    toolsPanel_.reset();
    canvas2_.reset();
    canvas_.reset();
    retired_.clear();
    docs_.clear();
    editor_ = nullptr;
    if (backends_) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
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

/* The part of the display a window may cover (0 x 0 when unknown). */
static WindowSize usableArea()
{
    SDL_Rect area = {0, 0, 0, 0};
    SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &area);
    return WindowSize{area.w, area.h};
}

bool App::createWindow(int width, int height)
{
    /* A window bigger than the screen (a small display, a large UI scale) would have its edges and
     * the status bar off screen, so the size is fitted to the usable area. */
    const WindowSize usable = usableArea();
    WindowSize fitted = window_fit(WindowSize{width, height}, usable);
    width = fitted.width;
    height = fitted.height;
    window_ = SDL_CreateWindow("tinysprite", width, height, SDL_WINDOW_RESIZABLE);
    if (window_) {
        WindowSize smallest = window_fit(window_minimum(theme::scale()), usable);
        SDL_SetWindowMinimumSize(window_, smallest.width, smallest.height);
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) {
            renderer_ = SDL_CreateRenderer(window_, SDL_SOFTWARE_RENDERER);
        }
        if (renderer_) {
            SDL_SetRenderVSync(renderer_, 1);
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
    ImGui::GetIO().ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard; /* Tab and arrows move focus */
    theme::load_fonts();                    /* on failure the built-in font stays */
    theme::apply();
    if (!ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_)) {
        return false;
    }
    if (!ImGui_ImplSDLRenderer3_Init(renderer_)) {
        ImGui_ImplSDL3_Shutdown();
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

float App::maxUiScale() const
{
    return window_max_scale(usableArea());
}

/* The scale comes from --scale, else the saved one, else what the display's DPI suggests. */
static float requestedScale(const AppOptions &options, const SettingsMap &settings)
{
    if (options.uiScale > 0.0f) {
        return options.uiScale;
    }
    float scale = 1.0f;
    if (settings_get_float(settings, "ui.scale", theme::SCALE_MIN, theme::SCALE_MAX, scale)) {
        return scale;
    }
    /* the content scale is 1 at 96 dpi, and 0 when unknown */
    return theme::scaleForDpi(SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay()) * 96.0f);
}

/* The saved window size; a default, scaled with the UI, when none was saved. */
static void initialWindowSize(const SettingsMap &settings, int &width, int &height)
{
    width = WINDOW_WIDTH;
    height = WINDOW_HEIGHT;
    settings_get_int(settings, "window.width", 320, 16384, width);
    settings_get_int(settings, "window.height", 240, 16384, height);
    if (settings.count("window.width") == 0) {
        width = static_cast<int>(theme::px(static_cast<float>(width)));
        height = static_cast<int>(theme::px(static_cast<float>(height)));
    }
}

/* Starts ImGui and makes the first document, a blank one of the size asked for, active in the
 * first pane; false, with a message, if either fails. */
bool App::startEditor(const AppOptions &options)
{
    if (initImGui()) {
        std::unique_ptr<Editor> first = Editor::create(renderer_, options.width, options.height);
        if (first) {
            docs_.push_back({std::move(first), "", nextSerial_++});
            editor_ = docs_.front().editor.get();
            pane_[0] = editor_;
        }
    }
    if (!editor_) {
        std::fprintf(stderr, "failed to initialise the editor\n");
        return false;
    }
    return true;
}

bool App::init(const AppOptions &options)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    /* Smoke runs (--frames, --script) neither read nor write the user's settings and autosave. */
    persist_ = (options.maxFrames == 0 && options.scriptPath.empty()) || !options.configDir.empty();
    if (!options.configDir.empty()) {
        configDir_ = options.configDir;
    } else if (persist_) {
        configDir_ = config_dir();
    }
    SettingsMap settings = settings_load(configDir_);
    /* The scale in use is kept to what the display can hold (a window cannot be smaller than the
     * layout needs), except for an explicit --scale; the one asked for stays what is saved. */
    theme::setScale(requestedScale(options, settings));
    if (options.uiScale <= 0.0f) {
        theme::cap_scale(window_max_scale(usableArea()));
    }
    int windowWidth = 0;
    int windowHeight = 0;
    initialWindowSize(settings, windowWidth, windowHeight);
    if (!createWindow(windowWidth, windowHeight)) {
        return false;
    }
    if (!startEditor(options)) {
        return false;
    }
    loadSettings(settings);
    canvas_ = std::make_unique<Canvas>(*editor_);
    canvas_->setOnPress([this] { focusPane(0); });
    toolsPanel_ = std::make_unique<ToolsPanel>(*editor_);
    layersPanel_ = std::make_unique<LayersPanel>(*editor_);
    layersPanel_->setContextMenu([this] { layerMenuItems(); });
    timelinePanel_ = std::make_unique<TimelinePanel>(*editor_);
    lastTick_ = SDL_GetTicks();
    if (!options.file.empty() && !openPath(options.file)) {
        std::fprintf(stderr, "%s\n", fileError_.c_str());
        return false;
    }
    if (persist_) {
        recovered_ = recovery_load(configDir_);
        openRecover_ = !recovered_.empty();
    }
    lastAutosave_ = SDL_GetTicks();
    return true;
}

/* "name.pxa" or "untitled", with a trailing "*" while there are unsaved changes. */
void App::updateTitle()
{
    std::string name = documentTitle(doc());
    std::string title = (dirty() ? name + "*" : name) + " - tinysprite";
    if (title != title_) {
        title_ = title;
        SDL_SetWindowTitle(window_, title_.c_str());
    }
}

/* Keeps the recovery copy of one document in step with it: dropped once it has no unsaved changes,
 * written when `due` (not in the middle of a stroke). True if a copy was written. */
bool App::autosaveDocument(Document &document, bool due)
{
    if (!dirty(document)) {
        if (document.recovery) {
            recovery_clear_slot(configDir_, document.serial);
            document.recovery = false;
        }
        return false;
    }
    if (!due || document.editor->stroking()) {
        return false;
    }
    document.recovery =
        recovery_save(configDir_, document.serial, document.editor->sprite(), document.path) ||
        document.recovery;
    return true;
}

/* While a document has unsaved changes, a copy of it goes to the config directory every so often
 * (one file per document, so editing a second one does not replace the first one's); once a
 * document has no unsaved changes any more its copy is not needed. */
void App::autosave()
{
    if (!persist_ || !recovered_.empty()) {
        return;
    }
    const Uint64 now = SDL_GetTicks();
    const bool due = now - lastAutosave_ >= AUTOSAVE_INTERVAL_MS;
    bool wrote = false;
    for (Document &document : docs_) {
        wrote = autosaveDocument(document, due) || wrote;
    }
    if (wrote) {
        lastAutosave_ = now;
    }
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
    settings_set_float(settings, "ui.scale", theme::requested_scale());
    settings["file.dir"] = lastDir_;
    settings_set_int(settings, "new.width", newWidth_);
    settings_set_int(settings, "new.height", newHeight_);
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window_, &width, &height);
    settings_set_int(settings, "window.width", width);
    settings_set_int(settings, "window.height", height);
    settings_save(configDir_, settings);
    if (recovered_.empty()) {
        recovery_clear(configDir_);
    }
}

void App::layersWindow(float height)
{
    if (panels_.begin(PanelId::Layers, height)) {
        toolsPanel_->colorBlock(panels_);
        layersPanel_->show(panels_);
    }
    panels_.end();
}

/* Reads the rendered frame back and writes it as an image chosen by the extension of `path` (a
 * development aid for checking the look without a display). */
bool App::saveScreenshot(const std::string &path) const
{
    SDL_Surface *read = SDL_RenderReadPixels(renderer_, nullptr);
    if (read == nullptr) {
        return false;
    }
    SDL_Surface *rgba = SDL_ConvertSurface(read, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(read);
    if (rgba == nullptr) {
        return false;
    }
    const int width = rgba->w;
    const int height = rgba->h;
    std::vector<Rgba> pixels(static_cast<size_t>(width) * static_cast<size_t>(height));
    for (int y = 0; y < height; y++) {
        const auto *row = static_cast<const uint8_t *>(rgba->pixels) +
                          static_cast<size_t>(y) * static_cast<size_t>(rgba->pitch);
        std::memcpy(&pixels[static_cast<size_t>(y) * static_cast<size_t>(width)], row,
                    static_cast<size_t>(width) * sizeof(Rgba));
    }
    SDL_DestroySurface(rgba);
    for (Rgba &pixel : pixels) {
        pixel.a = 255; /* the window is opaque */
    }
    return image_write_rgba(path, width, height, pixels);
}

/*
 * Tab with nothing navigable focused: at the start the canvas has the focus, and it has no control
 * to tab to, so Tab did nothing until a panel was clicked. Called with the frame's input queued
 * and not yet read, it moves the focus to the tool bar when a Tab press is waiting there, so that
 * same press lands on its first tool. The queue is what the window's key events and a script's
 * both end up in.
 */
static void focusPanelForTab()
{
    const ImGuiContext &g = *ImGui::GetCurrentContext();
    const bool onCanvas =
        g.NavWindow != nullptr && std::strncmp(g.NavWindow->Name, "Canvas", 6) == 0;
    if (g.NavId != 0 || !onCanvas) {
        return;
    }
    for (const ImGuiInputEvent &event : g.InputEventsQueue) {
        if (event.Type == ImGuiInputEventType_Key && event.Key.Key == ImGuiKey_Tab &&
            event.Key.Down) {
            ImGui::SetWindowFocus("Tools");
            return;
        }
    }
}

/* Feeds the window's events to ImGui and handles the ones the app owns. */
void App::pollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) {
            quit();
        } else if (event.type == SDL_EVENT_DROP_FILE) {
            dropFile(event.drop.data);
        }
    }
    focusPanelForTab();
}
/* Click handling of the timeline sash, after its invisible button was submitted. Returns whether
 * the sash is being dragged. */
bool App::sashClicked(Editor::Overlays &overlays)
{
    if (ImGui::IsItemActivated()) {
        sashStartHeight_ = overlays.timeline_height; /* what folding must not lose */
    }
    /* Two quick clicks restore the default height (and show the timeline if it is folded). The
     * first click already started a drag, which must not undo the reset while the button is
     * still held. */
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        overlays.timeline = true;
        overlays.timeline_height = TimelinePanel::BASE_HEIGHT;
        sashIgnoreDrag_ = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        sashIgnoreDrag_ = false;
    }
    return ImGui::IsItemActive() && !sashIgnoreDrag_;
}

/* The resize cursor and the accent line on the sash while it is hovered or dragged. */
static void sashFeedback(float x, float width, float edgeY, bool active)
{
    if (!active && !ImGui::IsItemHovered()) {
        return;
    }
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    ImGui::GetForegroundDrawList()->AddLine(
        ImVec2(x, edgeY), ImVec2(x + width, edgeY),
        ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::ACCENT)), theme::px(2.0f));
}

/* The edge of the timeline, like the edge of a VS Code panel: drag it up or down to resize the
 * timeline, drag it below the minimum to fold it away; while it is folded the same edge sits on
 * top of the status bar and dragging it up brings the timeline back. `bottomY` is where the
 * timeline ends (the top of the status bar), `maxHeight` how tall it may become. */
void App::timelineSash(float x, float width, float bottomY, float maxHeight)
{
    Editor::Overlays &overlays = editor_->overlays();
    const float scale = theme::scale();
    const float edgeY = bottomY - (overlays.timeline ? theme::px(overlays.timeline_height) : 0.0f);
    const float thickness = theme::px(SASH_THICKNESS);
    ImGui::SetNextWindowPos(ImVec2(x, edgeY - thickness / 2.0f));
    ImGui::SetNextWindowSize(ImVec2(width, thickness));
    constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoFocusOnAppearing |
                                       ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    /* without this a window is never smaller than 32 px: it would cover the timeline's top edge */
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.0f, 1.0f));
    if (ImGui::Begin("##timeline_sash", nullptr, FLAGS)) {
        ImGui::InvisibleButton("##sash", ImVec2(width, thickness));
        const bool active = sashClicked(overlays);
        sashFeedback(x, width, edgeY, active);
        if (active) {
            /* the limits and the stored height are at UI scale 1 */
            const TimelineSize size = timeline_drag((bottomY - ImGui::GetMousePos().y) / scale,
                                                    sashStartHeight_, TimelinePanel::MIN_HEIGHT,
                                                    maxHeight / scale, TimelinePanel::HIDE_BELOW);
            overlays.timeline = size.visible;
            overlays.timeline_height = size.height;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

/* Lays out and builds every panel and dialog of one UI frame. */
void App::buildUi()
{
    retired_.clear(); /* the previous frame, which drew them, is rendered */
    const bool controlsHaveKeys = keyboardOnControls(); /* before the menu bar takes a key */
    menuBar();
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    float left = 0.0f;
    float right = 0.0f;
    panels_.layout(left, right);
    float status = editor_->overlays().status ? status_bar_height() : 0.0f;
    float body = viewport->WorkSize.y - status; /* above the status bar */
    toolsPanel_->show(panels_, body);
    layersWindow(body);
    /* Every document a pane shows plays on; a tab that is not shown stands still. */
    Uint64 now = SDL_GetTicks();
    const int elapsed = static_cast<int>(now - lastTick_);
    pane_[0]->tick(elapsed);
    if (pane_[1] != nullptr && pane_[1] != pane_[0]) {
        pane_[1]->tick(elapsed);
    }
    lastTick_ = now;
    if (editor_->takeFramesFull()) {
        toast("A sprite can have at most " + std::to_string(SPRITE_MAX_FRAMES) + " frames", true);
    }
    if (editor_->takeMemoryShort()) {
        toast("Not enough memory for that: close layers, frames or tabs first", true);
    }
    const float tabs = tabBarHeight();
    tabBar(viewport->WorkPos.x + left, viewport->WorkPos.y, viewport->WorkSize.x - left - right,
           tabs);
    const float bar = editor_->overlays().options_bar ? theme::px(OPTIONS_BAR_HEIGHT) : 0.0f;
    const float maxTimeline = (body - tabs - bar) * TIMELINE_MAX_SHARE;
    float timeline = editor_->overlays().timeline
                         ? std::min(theme::px(editor_->overlays().timeline_height), maxTimeline)
                         : 0.0f;
    if (timeline > 0.0f) {
        timelinePanel_->show(viewport->WorkPos.x + left, viewport->WorkPos.y + body - timeline,
                             viewport->WorkSize.x - left - right, timeline);
    }
    if (bar > 0.0f) {
        toolsPanel_->contextBar(viewport->WorkPos.x + left, viewport->WorkPos.y + tabs,
                                viewport->WorkSize.x - left - right, bar);
    }
    canvasArea(viewport->WorkPos.x + left, viewport->WorkPos.y + tabs + bar,
               viewport->WorkSize.x - left - right, body - timeline - tabs - bar);

    if (status > 0.0f) {
        status_bar_show(*editor_, viewport->WorkPos.x, viewport->WorkPos.y + body,
                        viewport->WorkSize.x);
    }
    /* after the windows it overlaps, so it is the one under the mouse on the edge */
    timelineSash(viewport->WorkPos.x + left, viewport->WorkSize.x - left - right,
                 viewport->WorkPos.y + body, maxTimeline);
    toasts_.draw(ImGui::GetTime(), status + theme::px(16.0f));
    fileDialog();
    confirmDialog();
    newDialog();
    filterDialog();
    tilemapWindow_.show(renderer_, *editor_);
    recoverDialog();
    helpWindow();
    handleShortcuts(controlsHaveKeys);
    autosave();
    updateTitle();
}

/* Whether the frame just rendered is the last: the --frames count is reached or the script ran out.
 */
static bool isLastFrame(const AppOptions &options, int frame, bool scriptDone)
{
    return (options.maxFrames > 0 && frame + 1 >= options.maxFrames) || scriptDone;
}

/* The image to save after this frame: the script's `shot`, else --screenshot on the last frame; ""

 * * for none. */
static std::string shotPath(const ScriptInput &input, const AppOptions &options, bool lastFrame)
{
    if (!input.shot.empty()) {
        return input.shot;
    }
    return lastFrame ? options.screenshot : std::string();
}

void App::run(const AppOptions &options)
{
    int frames = 0;
    Script script = options.script;
    const bool scripted = !options.scriptPath.empty();
    while (running_) {
        pollEvents();
        ScriptInput input = scripted ? script.next() : ScriptInput();
        script_feed(ImGui::GetIO(), input);
        focusPanelForTab();
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        buildUi();

        SDL_SetRenderDrawColor(renderer_, (theme::WORKSPACE >> 16) & 0xff,
                               (theme::WORKSPACE >> 8) & 0xff, theme::WORKSPACE & 0xff, 255);
        SDL_RenderClear(renderer_);
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
        const bool lastFrame = isLastFrame(options, frames, scripted && script.done());
        const std::string shot = shotPath(input, options, lastFrame);
        if (!shot.empty() && !saveScreenshot(shot)) {
            std::fprintf(stderr, "cannot save %s\n", shot.c_str());
        }
        SDL_RenderPresent(renderer_);
        if (input.sleepMs > 0) {
            SDL_Delay(static_cast<Uint32>(input.sleepMs));
        }

        frames++;
        running_ = running_ && !lastFrame;
    }
    shutdown();
}