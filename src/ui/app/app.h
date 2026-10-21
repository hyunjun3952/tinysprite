#ifndef TINYSPRITE_UI_APP_H
#define TINYSPRITE_UI_APP_H

#include <SDL.h>

#include <memory>

#include "canvas/canvas.h"
#include "editor/editor.h"
#include "panels/layers_panel.h"
#include "panels/panels.h"
#include "panels/tools_panel.h"

struct AppOptions {
    int width = 64;
    int height = 64;
    int maxFrames = 0; /* >0: exit after this many frames (smoke runs without a display) */
};

/* Owns the window, the ImGui context and the editor, and runs the main loop. Everything it sets up
 * is released in the destructor, whichever step a failed start got through. */
class App
{
  public:
    App() = default;
    ~App();
    App(const App &) = delete;
    App &operator=(const App &) = delete;

    /* Prints the reason and returns false on failure. */
    bool init(const AppOptions &options);

    void run(const AppOptions &options);

  private:
    void menuBar();
    void plainShortcuts();
    void handleShortcuts();
    void layersWindow(float height);

    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    bool imguiContext_ = false;
    bool backends_ = false;
    bool running_ = true;

    Panels panels_;
    std::unique_ptr<Editor> editor_;
    std::unique_ptr<Canvas> canvas_;
    std::unique_ptr<ToolsPanel> toolsPanel_;
    std::unique_ptr<LayersPanel> layersPanel_;
};

#endif
