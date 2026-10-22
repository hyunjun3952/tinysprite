#ifndef TINYSPRITE_UI_APP_HPP
#define TINYSPRITE_UI_APP_HPP

#include <SDL.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "canvas.hpp"
#include "commands.hpp"
#include "editor.hpp"
#include "layers_panel.hpp"
#include "panels.hpp"
#include "recovery.hpp"
#include "settings.hpp"
#include "statusbar.hpp"
#include "timeline_panel.hpp"
#include "tools_panel.hpp"

struct AppOptions {
    int width = 64;
    int height = 64;
    int maxFrames = 0;      /* >0: exit after this many frames (smoke runs without a display) */
    std::string screenshot; /* with --frames: save the last frame to this image (.png, .bmp, .tga,
                               .jpg) before exiting */
    std::string file;       /* a .pxa or image to open at start, "" for a blank sprite */
    float uiScale = 0.0f;   /* the UI scale to use instead of the saved / detected one; 0 = none */
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
    App(App &&) = delete;
    App &operator=(App &&) = delete;

    /* Prints the reason and returns false on failure. */
    bool init(const AppOptions &options);

    void run(const AppOptions &options);

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
        ExportSheet,
        Count /* not an action: the number of them */
    };

    /* What the path dialog does for one FileAction (see fileInfo in app/files.cpp). */
    struct FileActionInfo {
        const char *title;
        const char *extension; /* added to a name typed without one; "" when the action reads */
        const char *done;      /* the start of the toast once it worked */
        bool (App::*run)(const std::string &path);
        void (App::*options)(); /* the dialog fields beyond the path, or null */
        bool writes() const;
    };
    static const FileActionInfo &fileInfo(FileAction action);

  private:
    friend struct AppTestAccess;             /* tests/ui drive whole frames */
    static constexpr size_t RECENT_MAX = 10; /* entries kept in the recent files list */

    bool createWindow(int width, int height);
    bool initImGui();
    void helpShortcut();
    void deleteShortcut();
    void pollEvents();
    void buildUi();
    void loadSettings(const SettingsMap &settings);

    void setUiScale(float scale);
    void uiScaleMenu();
    /* A message in the corner of the window that fades by itself; errors stay longer. */
    void toast(const std::string &text, bool error = false);

    void menuBar();
    void fileMenu();
    void exportMenu(bool idle);
    void paletteMenu(bool idle);
    void openRecentMenu(bool idle);
    void editMenu();
    void selectionMenu();
    void layerMenu();
    void layerMenuItems();
    void layerEditItems();
    void layerStateItems(bool idle);
    void frameMenu();
    void frameEditItems(bool idle);
    void celLinkItems(bool idle);
    void frameStepItems(bool idle);
    void viewMenu();
    /* The commands of commands.hpp: what each does, whether it can run now, and its menu item. */
    struct CommandAction {
        void (*run)(App &app);
        bool (*enabled)(const App &app);
    };
    static const CommandAction &commandAction(CommandId id);
    void runCommand(CommandId id);
    bool commandEnabled(CommandId id) const;
    /* A menu item for a command; `label` replaces the command's own when the item says something
     * more exact for the moment (e.g. "Merge layers" for a selection of several). */
    void commandItem(CommandId id, bool checked = false, const char *label = nullptr);
    void plainShortcuts();
    void nudgeShortcut();
    void ctrlShortcuts();
    void escapeShortcut();
    bool modalOpen() const;
    void handleShortcuts();
    void layersWindow(float height);
    void timelineSash(float x, float width, float bottomY, float maxHeight);
    float sashStartHeight_ = 0.0f; /* the timeline height when the current sash drag began */
    bool sashIgnoreDrag_ = false;  /* a double click just reset the height: ignore the drag */

    void askForPath(FileAction action);
    /* True when the dialog may write `path`: it is new, it is the document's own file, or the user
     * already saw the warning and pressed OK again. Otherwise arms the warning. */
    bool overwriteAllowed(const std::string &path);
    void fileDone(const std::string &path);
    void save();
    void fileDialog();
    void fileDialogMessages(const std::string &path);
    bool runFileAction(const FileActionInfo &info, const std::string &path);
    void closeFileDialog();
    bool pathField();
    bool runOpen(const std::string &path);
    bool runSaveAs(const std::string &path);
    bool runExportImage(const std::string &path);
    bool runExportGif(const std::string &path);
    bool runExportSheet(const std::string &path);
    bool runImportPalette(const std::string &path);
    bool runExportPalette(const std::string &path);
    void imageOptions();
    void animationOptions();
    void gifOptions();
    void sheetOptions();
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
    void saveAndProceed();
    void updateTitle();
    void dropFile(const std::string &path);
    /* Replaces the document with `path`; a failure reopens the Open dialog with the path filled in
     * and the reason shown. Used by dropped files and the recent list (after the unsaved check). */
    void openOrAsk(const std::string &path);
    void helpWindow();
    void newDialog();
    void newSizeFields();
    void recoverDialog();
    void autosave();
    /* Saves the preferences, the window size and the layout, and drops the autosave copy: a clean
     * exit leaves nothing to recover. */
    void shutdown();
    bool saveScreenshot(const std::string &path) const;

    std::string configDir_;              /* where they live, "" when there is no home directory */
    std::optional<Recovered> recovered_; /* the copy an earlier session left, awaiting an answer */
    std::vector<std::string> recent_;    /* recently opened / saved .pxa files, newest first */

    /* What the GIF and sprite sheet dialogs ask beyond the path and the scale. */
    struct ExportChoices {
        int tag = -1;   /* the tag they cover, -1 for all frames */
        int layout = 0; /* SheetLayout */
        int columns = 0;
        int spacing = 0;
        bool loop = true;
    };
    ExportChoices export_;
    int newWidth_ = 64;
    int newHeight_ = 64;
    int exportScale_ = 1;     /* the Export dialogs' enlargement */
    Uint32 lastAutosave_ = 0; /* SDL_GetTicks of the last autosave check */
    Uint32 lastTick_ = 0;     /* SDL_GetTicks of the previous frame, for playback */

    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;

    FileAction fileAction_ = FileAction::None;
    std::string filePath_;     /* the .pxa the document was opened from / saved to, "" if none */
    char pathInput_[512] = ""; /* the dialog's text field */
    std::string fileError_;
    std::string lastDir_; /* where the last file was opened / saved, for the next dialog */
    std::string
        overwritePath_; /* the path the dialog asked to overwrite and the user was warned about */
    Toasts toasts_;

    std::function<void()> pending_;   /* waits for the unsaved-changes popup */
    std::function<void()> afterSave_; /* runs once the Save as dialog it opened succeeds */
    std::string title_;

    Panels panels_;
    std::unique_ptr<Editor> editor_;
    std::unique_ptr<Canvas> canvas_;
    std::unique_ptr<ToolsPanel> toolsPanel_;
    std::unique_ptr<LayersPanel> layersPanel_;
    std::unique_ptr<TimelinePanel> timelinePanel_;

    bool persist_ = false;     /* keep settings and an autosave copy (off for --frames runs) */
    bool hasAutosave_ = false; /* an autosave copy of this session is on disk */
    bool openRecover_ = false;
    bool openNew_ = false;
    bool newShown_ = false;     /* the New sprite dialog is open (set while it is drawn) */
    bool recoverShown_ = false; /* ... and the recovery question */
    bool confirmShown_ = false; /* ... and the unsaved-changes question */
    bool imguiContext_ = false;
    bool backends_ = false;
    bool running_ = true;
    bool openFileDialog_ = false;
    bool openConfirm_ = false;
    bool showHelp_ = false;
};

#endif
