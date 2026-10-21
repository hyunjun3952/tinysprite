#ifndef TINYSPRITE_UI_APP_H
#define TINYSPRITE_UI_APP_H

#include <SDL.h>

#include <functional>
#include <memory>
#include <string>

#include "canvas.h"
#include "editor.h"
#include "layers_panel.h"
#include "panels.h"
#include "tools_panel.h"

struct AppOptions {
    int width = 64;
    int height = 64;
    int maxFrames = 0;      /* >0: exit after this many frames (smoke runs without a display) */
    std::string screenshot; /* with --frames: save the last frame to this .bmp before exiting */
    std::string file;       /* a .pxa or image to open at start, "" for a blank sprite */
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

    /* File menu: each action asks for a path in a small dialog (there is no native file chooser).
     */
    enum class FileAction { None, Open, SaveAs, ExportImage };
    void askForPath(FileAction action);
    void save();
    void fileDialog();
    bool runFileAction(const std::string &path);

    /* Opens a .pxa file or imports an image as the document; on failure sets fileError_. */
    bool openPath(const std::string &path);

    /* Unsaved changes: `proceed` runs at once if the document is saved, otherwise after the user
     * chose Save (then it runs once the save went through) or Discard in a confirmation popup. */
    bool dirty() const;
    void guardUnsaved(std::function<void()> proceed);
    void confirmDialog();
    void updateTitle();
    void dropFile(const std::string &path);
    void helpWindow();
    bool saveScreenshot(const std::string &path) const;

    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    bool imguiContext_ = false;
    bool backends_ = false;
    bool running_ = true;

    FileAction fileAction_ = FileAction::None;
    bool openFileDialog_ = false;
    std::string filePath_;     /* the .pxa the document was opened from / saved to, "" if none */
    char pathInput_[512] = ""; /* the dialog's text field */
    std::string fileError_;

    std::function<void()> pending_;   /* waits for the unsaved-changes popup */
    std::function<void()> afterSave_; /* runs once the Save as dialog it opened succeeds */
    bool openConfirm_ = false;
    bool showHelp_ = false;
    std::string title_;

    Panels panels_;
    std::unique_ptr<Editor> editor_;
    std::unique_ptr<Canvas> canvas_;
    std::unique_ptr<ToolsPanel> toolsPanel_;
    std::unique_ptr<LayersPanel> layersPanel_;
};

#endif
