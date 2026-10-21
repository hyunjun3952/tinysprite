#ifndef TINYSPRITE_UI_APP_H
#define TINYSPRITE_UI_APP_H

#include <SDL.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "canvas.h"
#include "commands.h"
#include "editor.h"
#include "layers_panel.h"
#include "panels.h"
#include "recovery.h"
#include "settings.h"
#include "timeline_panel.h"
#include "tools_panel.h"

struct AppOptions {
    int width = 64;
    int height = 64;
    int maxFrames = 0;      /* >0: exit after this many frames (smoke runs without a display) */
    std::string screenshot; /* with --frames: save the last frame to this .bmp before exiting */
    std::string file;       /* a .pxa or image to open at start, "" for a blank sprite */
    std::string configDir;  /* keep settings / autosave here (also in a --frames run); "" = the
                             * user's config directory, which a --frames run leaves alone */
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
    static constexpr size_t RECENT_MAX = 10; /* entries kept in the recent files list */

    bool createWindow(int width, int height);
    bool initImGui();
    void loadSettings(const SettingsMap &settings);

    void menuBar();
    void fileMenu();
    void openRecentMenu(bool idle);
    void editMenu();
    void frameMenu();
    void viewMenu();
    /* The commands of commands.h: what each does, whether it can run now, and its menu item. */
    void runCommand(CommandId id);
    bool commandEnabled(CommandId id) const;
    void commandItem(CommandId id);
    void plainShortcuts();
    void ctrlShortcuts();
    void escapeShortcut();
    bool modalOpen() const;
    void handleShortcuts();
    void layersWindow(float height);

    /* File menu: each action asks for a path in a small dialog (there is no native file chooser).
     */
    enum class FileAction {
        None,
        Open,
        SaveAs,
        ExportImage,
        ImportPalette,
        ExportPalette,
        ExportGif,
        ExportSheet
    };
    void askForPath(FileAction action);
    void save();
    void fileDialog();
    bool runFileAction(const std::string &path);
    void exportOptions();
    /* The frames and direction the animation exports cover: all frames, or the tag picked in the
     * dialog (its own direction). */
    void exportRange(int &first, int &last, PlayMode &direction) const;

    /* Opens a .pxa file or imports an image as the document; on failure sets fileError_. */
    bool openPath(const std::string &path);

    /* Unsaved changes: `proceed` runs at once if the document is saved, otherwise after the user
     * chose Save (then it runs once the save went through) or Discard in a confirmation popup. */
    bool dirty() const;
    void guardUnsaved(std::function<void()> proceed);
    void confirmDialog();
    void updateTitle();
    void dropFile(const std::string &path);
    /* Replaces the document with `path`; a failure reopens the Open dialog with the path filled in
     * and the reason shown. Used by dropped files and the recent list (after the unsaved check). */
    void openOrAsk(const std::string &path);
    void helpWindow();
    void newDialog();
    void recoverDialog();
    void autosave();
    /* Saves the preferences, the window size and the layout, and drops the autosave copy: a clean
     * exit leaves nothing to recover. */
    void shutdown();
    bool saveScreenshot(const std::string &path) const;

    bool persist_ = false;     /* keep settings and an autosave copy (off for --frames runs) */
    std::string configDir_;    /* where they live, "" when there is no home directory */
    Uint32 lastAutosave_ = 0;  /* SDL_GetTicks of the last autosave check */
    bool hasAutosave_ = false; /* an autosave copy of this session is on disk */
    std::optional<Recovered> recovered_; /* the copy an earlier session left, awaiting an answer */
    bool openRecover_ = false;
    bool openNew_ = false;
    bool newShown_ = false;     /* the New sprite dialog is open (set while it is drawn) */
    bool recoverShown_ = false; /* ... and the recovery question */
    int newWidth_ = 64;
    int newHeight_ = 64;
    int exportScale_ = 1; /* the Export dialogs' enlargement */
    int exportTag_ = -1;  /* the tag the GIF / sheet covers, -1 for all frames */
    bool gifLoop_ = true;
    int sheetLayout_ = 0; /* SheetLayout */
    int sheetColumns_ = 0;
    int sheetSpacing_ = 0;
    std::vector<std::string> recent_; /* recently opened / saved .pxa files, newest first */

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
    std::unique_ptr<TimelinePanel> timelinePanel_;
    Uint32 lastTick_ = 0; /* SDL_GetTicks of the previous frame, for playback */
};

#endif
