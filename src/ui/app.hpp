#ifndef TINYSPRITE_UI_APP_HPP
#define TINYSPRITE_UI_APP_HPP

#include <SDL.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "canvas.hpp"
#include "commands.hpp"
#include "editor.hpp"
#include "file_browser.hpp"
#include "layers_panel.hpp"
#include "panels.hpp"
#include "recovery.hpp"
#include "settings.hpp"
#include "statusbar.hpp"
#include "theme.hpp"
#include "tilemap_window.hpp"
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
        const char *filter;     /* the extensions the file list shows, e.g. ".png .gif"; "" = all */
        bool writes() const;
    };
    static const FileActionInfo &fileInfo(FileAction action);

  private:
    friend struct AppTestAccess;                    /* tests/ui drive whole frames */
    static constexpr float TREE_ROW_PADDING = 5.0f; /* above and below a row of the file tree */
    static constexpr size_t RECENT_MAX = 10;        /* entries kept in the recent files list */

    bool createWindow(int width, int height);
    bool initImGui();
    void helpShortcut();
    void deleteShortcut();
    void pollEvents();
    void buildUi();
    void loadSettings(const SettingsMap &settings);

    void setUiScale(float scale);
    float maxUiScale() const; /* the largest scale whose minimum window fits this display */
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
    void filterMenu();
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
    bool sashClicked(Editor::Overlays &overlays);
    float sashStartHeight_ = 0.0f; /* the timeline height when the current sash drag began */
    bool sashIgnoreDrag_ = false;  /* a double click just reset the height: ignore the drag */

    void askForPath(FileAction action);
    /* True when the dialog may write `path`: it is new, it is the document's own file, or the user
     * already saw the warning and pressed OK again. Otherwise arms the warning. */
    bool overwriteAllowed(const std::string &path);
    /* The file `path` would overwrite: itself, or the .json a sprite sheet writes beside its image;
     * "" when neither exists. */
    std::string existingOutput(const std::string &path) const;
    void fileDone(const std::string &path);
    void save();
    void fileDialog();
    std::string dialogTarget(const FileActionInfo &info);
    void fileDialogMessages();
    bool runFileAction(const FileActionInfo &info, const std::string &path);
    void closeFileDialog();
    bool pathField();

    /* The folder tree and search under the path field (app/file_browser_ui.cpp): a folder's name
     * picks it as the place to save to and its arrow opens it, a file goes into the path field.
     * Returns true when a file was double clicked in a dialog that reads (it counts as OK). */
    bool fileBrowser(const FileActionInfo &info);
    /* The height of a row of the file tree. */
    static float treeRowHeight();
    bool treeRows(const FileActionInfo &info, const std::string &directory, int depth);
    bool fileRows(const FileActionInfo &info, const std::string &directory,
                  const std::vector<DirEntry> &entries, size_t from, size_t to,
                  const std::string &current);
    bool folderNode(const FileActionInfo &info, const std::string &path, const std::string &name,
                    bool selected, int depth);
    bool fileRow(const FileActionInfo &info, const std::string &path, const std::string &name,
                 bool selected);
    bool searchRows(const FileActionInfo &info);
    void browseToolbar();
    void requestOpenAll(int direction);
    bool pickFile(const FileActionInfo &info, const std::string &path);
    const std::vector<DirEntry> &listing(const std::string &directory, const FileActionInfo &info);
    void browseTo(const std::string &directory);
    /* Where the dialog starts and the list falls back to: the last folder, else the current one. */
    std::string startDirectory() const;
    /* The path the dialog would use: the typed text with "~" expanded and the action's extension.
     */
    std::string dialogPath(const FileActionInfo &info, const std::string &typed) const;
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

    /* "Cannot write <path>" plus the OS reason of the last failed file write, if there is one. */
    static std::string cannotWrite(const std::string &path, const std::string &hint);

    /* One open document, shown as a tab. Every document has its own Editor (history, selection,
     * view, texture); `editor_` points at the active one's, which the panels and menus work on. */
    struct Document {
        std::unique_ptr<Editor> editor;
        std::string path; /* the .pxa it was opened from / saved to, "" if none */
        int serial =
            0; /* names an untitled one: "untitled", "untitled 2", ...; also its autosave slot */
        bool recovery = false; /* an autosave copy of it is on disk */
    };

    /* Opens a .pxa file or imports an image in a new tab (an untouched untitled tab is reused, and
     * a file that is already open is just shown); on failure sets fileError_. */
    bool openPath(const std::string &path);

    /* The documents (app/documents.cpp). The active one is docs_[active_] and editor_ points at
     * its Editor. */
    Document &doc(int index)
    {
        return docs_[static_cast<size_t>(index)];
    }
    const Document &doc(int index) const
    {
        return docs_[static_cast<size_t>(index)];
    }
    Document &doc()
    {
        return doc(active_);
    }
    const Document &doc() const
    {
        return doc(active_);
    }
    std::string &filePath()
    {
        return doc().path;
    }
    bool dirty(const Document &document) const;
    /* An untitled tab nobody has edited: opening a file reuses it instead of adding a tab. */
    bool pristine(const Document &document) const;
    std::string documentTitle(const Document &document) const;
    int findDocument(const std::string &path) const;
    /* The tab that holds `editor`, or -1; and taking that tab away (it may have moved meanwhile).
     */
    int indexOf(const Editor *editor) const;
    void removeEditor(const Editor *editor);
    /* Adds a tab for `sprite` (or reuses the untouched active one when `reuse`), and shows it. */
    bool addDocument(Sprite sprite, const std::string &path, bool reuse);
    bool addBlankDocument(int width, int height);
    /* Shows tab `index`: the panels follow, and the tool, colors and clipboard come along. Refused
     * while a tool is being dragged. */
    void activateDocument(int index);
    /* Moves tab `from` so it ends up at `to`; the active tab stays the same document. */
    void moveDocument(int from, int to);
    bool validIndex(int index) const;
    bool keepOneDocument();
    /* Takes tab `index` away without asking; a last tab is replaced by a blank one. */
    void removeDocument(int index);
    /* Closes tab `index`, asking first when it has unsaved changes. */
    void closeDocument(int index);
    /* The tab bar (app/tabs.cpp): drawn over [x, x + width] x [y, y + height]; its height is
     * tabBarHeight(). */
    void tabBar(float x, float y, float width, float height);
    static float tabBarHeight();
    void tabItem(int index, bool forced, int &picked, int &closeRequest, ImGuiID &id);
    void syncTabOrder(const std::vector<ImGuiID> &ids);
    void tabMenu(int index, int &closeRequest);
    void closeMarked();
    void closeAfter(const Editor *keep, bool rightOnly);
    int nextToClose(const Editor *keep, bool rightOnly) const;
    /* The split view (app/split.cpp): the canvas area holds two panes, each showing a document. A
     * click in a pane makes its document the active one; picking a tab fills the active pane. */
    bool splitView(int rightDocument, bool sideBySide);
    void unsplit();
    void focusPane(int pane);
    void bindPanes();
    void canvasArea(float x, float y, float width, float height);
    void splitSash(float x, float y, float width, float height, float thickness);
    void dragSash(float offset, float total, float thickness);
    void sashHandle(ImVec2 size, ImVec2 origin, float total, float thickness);
    /* Leaves the app once every tab with unsaved changes was saved or discarded. */
    void quit();

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
    void filterDialog();
    void filterFields();
    void openFilter(FilterParams::Kind kind);
    bool runFilter();
    void newSizeFields();
    void recoverDialog();
    void recoverList();
    bool recoverAll();
    void autosave();
    /* Saves the preferences, the window size and the layout, and drops the autosave copy: a clean
     * exit leaves nothing to recover. */
    void shutdown();
    bool saveScreenshot(const std::string &path) const;

    std::string configDir_;            /* where they live, "" when there is no home directory */
    std::vector<Recovered> recovered_; /* the copies an earlier session left, awaiting an answer */
    std::vector<std::string> recent_;  /* recently opened / saved .pxa files, newest first */

    /* What the GIF and sprite sheet dialogs ask beyond the path and the scale. */
    struct ExportChoices {
        int tag = -1;   /* the tag they cover, -1 for all frames */
        int layout = 0; /* SheetLayout */
        int columns = 0;
        int spacing = 0;
        bool loop = true;
        bool json = true; /* the sprite sheet dialog also writes the metadata beside the image */
    };
    ExportChoices export_;
    /* The Filter dialog: which filter, its colors as floats for the hex fields (outline, replace
     * from, replace to) and whether it covers every frame. */
    FilterParams filter_;
    float filterColors_[3][4] = {};
    char filterHex_[3][theme::HEX_BUFFER_SIZE] = {};
    bool filterAllFrames_ = false;
    int newWidth_ = 64;
    int newHeight_ = 64;
    int exportScale_ = 1;     /* the Export dialogs' enlargement */
    Uint32 lastAutosave_ = 0; /* SDL_GetTicks of the last autosave check */
    Uint32 lastTick_ = 0;     /* SDL_GetTicks of the previous frame, for playback */

    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;

    FileAction fileAction_ = FileAction::None;
    char pathInput_[512] = ""; /* the dialog's text field */
    std::string browseRoot_;   /* the folder the tree starts at; "" = not chosen yet */
    std::map<std::string, std::vector<DirEntry>> browseCache_; /* folder -> its entries */
    std::vector<DirEntry> browseFound_;                        /* the search results ... */
    std::string browseFoundKey_;                               /* ... and what they are for */
    int browseOpenAll_ = 0; /* +1: open every folder of the tree this frame, -1: close them all */
    int browseOpenBudget_ = 0;   /* how many more folders an "open all" may open */
    bool browserOpen_ = false;   /* the folder button of the dialog is on */
    char browseSearch_[64] = ""; /* the list shows the names containing this */
    std::string fileError_;
    std::string lastDir_; /* where the last file was opened / saved, for the next dialog */
    std::string
        overwritePath_; /* the path the dialog asked to overwrite and the user was warned about */
    std::string overwriteName_; /* the file name that warning named (the image, or its .json) */
    Toasts toasts_;

    std::function<void()> pending_;   /* waits for the unsaved-changes popup */
    std::function<void()> afterSave_; /* runs once the Save as dialog it opened succeeds */
    std::string title_;

    Panels panels_;
    std::vector<Document> docs_;
    /* Editors of closed tabs: the frame being built may have drawn their textures, so they are
     * destroyed at the start of the next frame, after that one was rendered. */
    std::vector<std::unique_ptr<Editor>> retired_;
    int active_ = 0;
    int nextSerial_ = 1;
    bool split_ = false;                   /* two panes show documents */
    bool splitSideBySide_ = true;          /* left | right, else top / bottom */
    float splitRatio_ = 0.5f;              /* the first pane's share of the canvas area */
    Editor *pane_[2] = {nullptr, nullptr}; /* what each pane shows (pane_[0] only when not split) */
    int activePane_ = 0;
    std::unique_ptr<Canvas> canvas2_;      /* the second pane's canvas, made with the first split */
    int tabSelected_ = -1;                 /* the tab the tab bar was last told is selected */
    const Editor *closeOthers_ = nullptr;  /* set by a tab's menu: close every other tab */
    const Editor *closeRightOf_ = nullptr; /* ... or every tab after this one */
    Editor *editor_ = nullptr;
    std::unique_ptr<Canvas> canvas_;
    std::unique_ptr<ToolsPanel> toolsPanel_;
    std::unique_ptr<LayersPanel> layersPanel_;
    std::unique_ptr<TimelinePanel> timelinePanel_;
    TilemapWindow tilemapWindow_; /* its textures go before the renderer, see ~App */

    bool persist_ = false; /* keep settings and an autosave copy (off for --frames runs) */
    bool openRecover_ = false;
    bool openNew_ = false;
    bool newShown_ = false; /* the New sprite dialog is open (set while it is drawn) */
    bool openFilter_ = false;
    bool filterShown_ = false;  /* the Filter dialog is open (set while it is drawn) */
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
