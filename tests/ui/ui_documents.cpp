/* The open documents of the app (tabs): each has its own Editor, a file is opened in a tab, closing
 * asks about unsaved changes and the tool, colors and clipboard follow the user between tabs. */
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "app.hpp"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "imgui_internal.h"
#include "minitest.hpp"
#include "recovery.hpp"
#include "tinysprite/io.hpp"

struct AppTestAccess {
    App app;

    bool start(const std::string &configDir = "")
    {
        AppOptions options;
        options.width = 16;
        options.height = 16;
        options.maxFrames = 1;         /* no settings or autosave of the user's */
        options.configDir = configDir; /* a test folder turns them on there */
        if (!app.init(options)) {
            return false;
        }
        for (int i = 0; i < 3; i++) {
            frame();
        }
        return true;
    }
    void frame()
    {
        app.pollEvents();
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        app.buildUi();
        ImGui::Render();
    }
    int count() const
    {
        return static_cast<int>(app.docs_.size());
    }
    int active() const
    {
        return app.active_;
    }
    Editor &editor()
    {
        return *app.editor_;
    }
    Editor &editorOf(int index)
    {
        return *app.docs_[static_cast<size_t>(index)].editor;
    }
    std::string title(int index) const
    {
        return app.documentTitle(app.docs_[static_cast<size_t>(index)]);
    }
    /* Paints one red pixel on the active document with the pencil. */
    void dot(int x, int y)
    {
        Editor &e = editor();
        e.foregroundFloats()[0] = 1.0f;
        e.foregroundFloats()[3] = 1.0f;
        e.selectTool(ToolKind::Pencil);
        e.press(x, y);
        e.release();
    }
    int alphaAt(int index, int x, int y)
    {
        return editorOf(index).sprite().layer(0)->cel(0)->get(x, y).a;
    }
    bool asking() const
    {
        return app.openConfirm_;
    }
    /* What the Discard button does. */
    void discard()
    {
        std::function<void()> proceed = std::move(app.pending_);
        app.pending_ = nullptr;
        app.openConfirm_ = false;
        proceed();
    }
    bool running() const
    {
        return app.running_;
    }
    bool dirty() const
    {
        return app.dirty();
    }
    bool addBlank(int width, int height)
    {
        return app.addBlankDocument(width, height);
    }
    bool open(const std::string &path)
    {
        return app.openPath(path);
    }
    void activate(int index)
    {
        app.activateDocument(index);
    }
    void close(int index)
    {
        app.closeDocument(index);
    }
    void quit()
    {
        app.quit();
    }
    /* The sprite sheet dialog: would writing `path` overwrite something the user was not asked
     * about? */
    bool sheetOverwriteAllowed(const std::string &path, bool alsoJson)
    {
        app.fileAction_ = App::FileAction::ExportSheet;
        app.export_.json = alsoJson;
        app.overwritePath_.clear();
        return app.overwriteAllowed(path);
    }
    bool overwriteAllowedAgain(const std::string &path)
    {
        return app.overwriteAllowed(path);
    }
    std::string overwriteName() const
    {
        return app.overwriteName_;
    }
    void drop(const std::string &path)
    {
        app.dropFile(path);
    }
    void pressSave()
    {
        app.saveAndProceed();
    }
    void setPath(const std::string &path)
    {
        app.filePath() = path;
    }
    void move(int from, int to)
    {
        app.moveDocument(from, to);
    }
    void autosaveNow()
    {
        app.lastAutosave_ = 0xF0000000u; /* long ago: the next autosave is due */
        app.autosave();
    }
    void recoverAll(std::vector<Recovered> copies)
    {
        app.recovered_ = std::move(copies);
        app.recoverAll();
        app.recovered_.clear();
    }
    bool saveAs(const std::string &path)
    {
        return app.runSaveAs(path);
    }
    std::string error() const
    {
        return app.fileError_;
    }
    bool pressed(int x, int y) /* a tool drag that is not released yet */
    {
        editor().selectTool(ToolKind::Pencil);
        editor().press(x, y);
        return editor().stroking();
    }
    void release()
    {
        editor().release();
    }
    /* Editors of closed tabs that wait for the next frame to be destroyed. */
    int retired() const
    {
        return static_cast<int>(app.retired_.size());
    }
    /* The split view. */
    bool split() const
    {
        return app.split_;
    }
    bool splitView(bool sideBySide)
    {
        return app.splitView(-1, sideBySide);
    }
    void unsplit()
    {
        app.unsplit();
    }
    int activePane() const
    {
        return app.activePane_;
    }
    /* The index of the document shown in pane `pane` (the first pane shows the active one when
     * there is no split). */
    int paneDoc(int pane) const
    {
        for (size_t i = 0; i < app.docs_.size(); i++) {
            if (app.docs_[i].editor.get() == app.pane_[pane]) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
    /* A click in the middle of a window of that name. */
    void clickWindow(const char *name)
    {
        const ImGuiWindow *w = ImGui::FindWindowByName(name);
        ImVec2 at = w ? ImVec2(w->Pos.x + w->Size.x / 2.0f, w->Pos.y + w->Size.y / 2.0f)
                      : ImVec2(-1.0f, -1.0f);
        ImGuiIO &io = ImGui::GetIO();
        io.AddMousePosEvent(at.x, at.y);
        frame();
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
        frame();
    }
    ImVec2 windowSize(const char *name) const
    {
        const ImGuiWindow *w = ImGui::FindWindowByName(name);
        return w ? w->Size : ImVec2(0.0f, 0.0f);
    }
};

static std::string temp_pxa(const char *name, int size)
{
    std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    Sprite sprite(size, size);
    pxa_save(sprite, path.string());
    return path.string();
}

TEST(the_app_starts_with_one_untitled_document)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_EQ_INT(1, t.count());
    ASSERT_EQ_INT(0, t.active());
    ASSERT_TRUE(t.title(0) == "untitled");
    ASSERT_TRUE(!t.dirty());
}

TEST(a_new_document_is_a_new_tab_and_is_shown)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 6));
    ASSERT_EQ_INT(2, t.count());
    ASSERT_EQ_INT(1, t.active());
    ASSERT_EQ_INT(8, t.editor().sprite().width());
    ASSERT_EQ_INT(6, t.editor().sprite().height());
    ASSERT_TRUE(t.title(1) == "untitled 2");
    t.frame(); /* a frame with the second document works */
}

TEST(opening_a_file_reuses_an_untouched_untitled_tab_then_adds_tabs)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    std::string a = temp_pxa("tinysprite_doc_a.pxa", 5);
    std::string b = temp_pxa("tinysprite_doc_b.pxa", 7);
    ASSERT_TRUE(t.open(a));
    ASSERT_EQ_INT(1, t.count()); /* the blank tab nobody touched was reused */
    ASSERT_EQ_INT(5, t.editor().sprite().width());
    ASSERT_TRUE(t.title(0) == "tinysprite_doc_a.pxa");
    ASSERT_TRUE(t.open(b));
    ASSERT_EQ_INT(2, t.count());
    ASSERT_EQ_INT(1, t.active());
    ASSERT_TRUE(t.open(a)); /* already open: shown, not opened again */
    ASSERT_EQ_INT(2, t.count());
    ASSERT_EQ_INT(0, t.active());
    std::filesystem::remove(a);
    std::filesystem::remove(b);
}

TEST(an_edited_untitled_tab_is_kept_when_a_file_is_opened)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    t.dot(1, 1);
    std::string a = temp_pxa("tinysprite_doc_c.pxa", 5);
    ASSERT_TRUE(t.open(a));
    ASSERT_EQ_INT(2, t.count());
    ASSERT_EQ_INT(255, t.alphaAt(0, 1, 1)); /* the drawing is still there */
    std::filesystem::remove(a);
}

TEST(each_tab_keeps_its_own_pixels_and_undo_history)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    t.dot(2, 2);
    ASSERT_TRUE(t.addBlank(16, 16));
    ASSERT_EQ_INT(0, t.alphaAt(1, 2, 2));
    ASSERT_TRUE(!t.editor().history().canUndo());
    t.activate(0);
    ASSERT_EQ_INT(255, t.alphaAt(0, 2, 2));
    ASSERT_TRUE(t.editor().history().canUndo());
    t.editor().undo();
    ASSERT_EQ_INT(0, t.alphaAt(0, 2, 2));
}

TEST(the_tool_and_the_clipboard_follow_the_user_from_tab_to_tab)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    t.dot(1, 1);
    t.editor().selectAll();
    ASSERT_TRUE(t.editor().copySelection());
    t.editor().selectTool(ToolKind::Bucket);
    ASSERT_TRUE(t.addBlank(16, 16));
    ASSERT_EQ_INT(static_cast<int>(ToolKind::Bucket), static_cast<int>(t.editor().tool()));
    t.editor().paste(); /* the copy made in the first tab */
    ASSERT_EQ_INT(255, t.alphaAt(1, 1, 1));
}

TEST(closing_a_clean_tab_removes_it_and_the_last_one_is_replaced_by_a_blank)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 8));
    t.close(1);
    ASSERT_EQ_INT(1, t.count());
    ASSERT_EQ_INT(0, t.active());
    t.close(0);
    ASSERT_EQ_INT(1, t.count()); /* there is always a document */
    ASSERT_TRUE(!t.dirty());
    t.frame();
}

TEST(closing_the_tabs_before_the_active_one_keeps_it_active)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 8));
    ASSERT_TRUE(t.addBlank(9, 9));
    ASSERT_EQ_INT(2, t.active());
    t.close(0);
    ASSERT_EQ_INT(1, t.active());
    ASSERT_EQ_INT(9, t.editor().sprite().width());
    t.frame();
}

TEST(closing_a_tab_with_unsaved_changes_asks_first)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 8));
    t.dot(1, 1); /* tab 1 has unsaved changes */
    t.activate(0);
    t.close(1);
    ASSERT_EQ_INT(2, t.count());
    ASSERT_TRUE(t.asking());
    ASSERT_EQ_INT(1, t.active()); /* the question is about the tab being closed: it is shown */
    t.discard();
    ASSERT_EQ_INT(1, t.count());
}

TEST(quitting_asks_about_every_tab_with_unsaved_changes)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    t.dot(1, 1);
    ASSERT_TRUE(t.addBlank(8, 8));
    t.dot(2, 2);
    t.quit();
    ASSERT_TRUE(t.asking());
    ASSERT_TRUE(t.running());
    t.discard(); /* the first tab with changes goes, and the next one is asked about */
    ASSERT_TRUE(t.asking());
    ASSERT_TRUE(t.running());
    t.discard();
    ASSERT_TRUE(!t.running());
}

TEST(quitting_with_nothing_unsaved_leaves_at_once)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 8));
    t.quit();
    ASSERT_TRUE(!t.running());
}

TEST(tabs_can_be_reordered_and_the_active_one_stays_the_same_document)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 8));
    ASSERT_TRUE(t.addBlank(9, 9));
    ASSERT_EQ_INT(2, t.active()); /* the 9 x 9 one */
    t.move(2, 0);
    ASSERT_EQ_INT(0, t.active()); /* it moved, and is still the one shown */
    ASSERT_EQ_INT(9, t.editorOf(0).sprite().width());
    ASSERT_EQ_INT(16, t.editorOf(1).sprite().width());
    ASSERT_EQ_INT(8, t.editorOf(2).sprite().width());
    t.move(1, 2);
    ASSERT_EQ_INT(0, t.active());
    ASSERT_EQ_INT(8, t.editorOf(1).sprite().width());
    ASSERT_EQ_INT(16, t.editorOf(2).sprite().width());
    t.move(0, 5); /* out of range: nothing happens */
    ASSERT_EQ_INT(9, t.editorOf(0).sprite().width());
}

TEST(a_split_needs_a_second_document_and_shows_it_beside_the_first)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(!t.splitView(true)); /* one document: nothing to put in the second pane */
    ASSERT_TRUE(!t.split());
    ASSERT_TRUE(t.addBlank(8, 8));
    t.activate(0);
    ASSERT_TRUE(t.splitView(true));
    ASSERT_TRUE(t.split());
    ASSERT_EQ_INT(0, t.paneDoc(0));
    ASSERT_EQ_INT(1, t.paneDoc(1));
    ASSERT_EQ_INT(0, t.activePane());
    t.frame();
    t.frame();
    ImVec2 left = t.windowSize("Canvas");
    ImVec2 right = t.windowSize("Canvas 2");
    ASSERT_TRUE(left.x > 0.0f && right.x > 0.0f);
    ASSERT_TRUE(left.y == right.y); /* side by side: the same height */
    t.unsplit();
    ASSERT_TRUE(!t.split());
    ASSERT_EQ_INT(0, t.paneDoc(0));
}

TEST(a_stacked_split_gives_both_panes_the_full_width)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 8));
    t.activate(0);
    ASSERT_TRUE(t.splitView(false));
    t.frame();
    t.frame();
    ASSERT_TRUE(t.windowSize("Canvas").x == t.windowSize("Canvas 2").x);
}

TEST(clicking_a_pane_makes_its_document_the_active_one)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 8));
    t.activate(0);
    ASSERT_TRUE(t.splitView(true));
    t.frame();
    t.frame();
    t.clickWindow("Canvas 2");
    ASSERT_EQ_INT(1, t.activePane());
    ASSERT_EQ_INT(1, t.active());
    t.clickWindow("Canvas");
    ASSERT_EQ_INT(0, t.activePane());
    ASSERT_EQ_INT(0, t.active());
}

TEST(picking_a_tab_fills_the_active_pane_and_a_tab_in_the_other_pane_just_focuses_it)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 8));
    ASSERT_TRUE(t.addBlank(9, 9));
    t.activate(0);
    ASSERT_TRUE(t.splitView(true)); /* panes: tab 0 and tab 1 */
    t.activate(2);                  /* the first pane is active: it takes tab 2 */
    ASSERT_EQ_INT(2, t.paneDoc(0));
    ASSERT_EQ_INT(1, t.paneDoc(1));
    ASSERT_EQ_INT(0, t.activePane());
    t.activate(1); /* already in the other pane: that pane is focused, nothing is replaced */
    ASSERT_EQ_INT(2, t.paneDoc(0));
    ASSERT_EQ_INT(1, t.paneDoc(1));
    ASSERT_EQ_INT(1, t.activePane());
    ASSERT_EQ_INT(1, t.active());
}

TEST(closing_a_tab_that_is_shown_in_a_pane_ends_the_split)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 8));
    ASSERT_TRUE(t.addBlank(9, 9));
    t.activate(0);
    ASSERT_TRUE(t.splitView(true)); /* tabs 0 and 1 */
    t.close(1);
    ASSERT_TRUE(!t.split());
    ASSERT_EQ_INT(0, t.active());
    t.frame();
    ASSERT_TRUE(t.splitView(true)); /* the remaining two can be split again */
    t.close(2 - 1);                 /* tab 1 is the old tab 2 now */
    ASSERT_TRUE(!t.split());
}

TEST(closing_a_tab_that_is_not_shown_keeps_the_split)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 8));
    ASSERT_TRUE(t.addBlank(9, 9));
    t.activate(0);
    ASSERT_TRUE(t.splitView(true)); /* tabs 0 and 1 */
    t.close(2);
    ASSERT_TRUE(t.split());
    ASSERT_EQ_INT(0, t.paneDoc(0));
    ASSERT_EQ_INT(1, t.paneDoc(1));
}

/* The frame being built already drew the closed tab's texture (and its layer thumbnails); they are
 * rendered at the end of the frame, so the editor must outlive it. */
TEST(a_closed_tabs_editor_is_kept_until_the_next_frame)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 8));
    t.close(1);
    ASSERT_EQ_INT(1, t.retired());
    t.frame();
    ASSERT_EQ_INT(0, t.retired());
}

TEST(each_document_with_changes_gets_its_own_autosave_copy)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "tinysprite_autosave_test";
    fs::remove_all(dir);
    AppTestAccess t;
    ASSERT_TRUE(t.start(dir.string()));
    t.dot(1, 1);
    ASSERT_TRUE(t.addBlank(8, 8));
    t.dot(2, 2);
    t.autosaveNow();
    ASSERT_EQ_INT(
        2, static_cast<int>(recovery_load(dir.string()).size())); /* not one over the other */
    t.activate(0);
    t.editor().undo(); /* the first document is clean again: its copy goes */
    t.autosaveNow();
    ASSERT_EQ_INT(1, static_cast<int>(recovery_load(dir.string()).size()));
    t.close(1); /* closing (discarding) the other one drops its copy too */
    t.discard();
    ASSERT_EQ_INT(0, static_cast<int>(recovery_load(dir.string()).size()));
    fs::remove_all(dir);
}

TEST(recovered_copies_open_as_tabs_and_never_share_a_file)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    std::vector<Recovered> copies;
    copies.push_back(Recovered{Sprite(5, 5), "/work/a.pxa"});
    copies.push_back(Recovered{Sprite(6, 6), "/work/a.pxa"}); /* the same file again */
    copies.push_back(Recovered{Sprite(7, 7), ""});
    t.recoverAll(std::move(copies));
    ASSERT_EQ_INT(3, t.count()); /* the untouched first tab took the first copy */
    ASSERT_EQ_INT(5, t.editorOf(0).sprite().width());
    ASSERT_TRUE(t.title(0) == "a.pxa");
    ASSERT_TRUE(t.title(1) != "a.pxa"); /* the second one has no file: Save as picks one */
    for (int i = 0; i < 3; i++) {
        t.activate(i);
        ASSERT_TRUE(t.dirty());
    }
}

TEST(save_as_onto_the_file_of_another_tab_is_refused)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    std::string a = temp_pxa("tinysprite_doc_saveas.pxa", 5);
    ASSERT_TRUE(t.open(a));
    ASSERT_TRUE(t.addBlank(8, 8));
    ASSERT_TRUE(!t.saveAs(a));
    ASSERT_TRUE(t.error().find("open in another tab") != std::string::npos);
    std::filesystem::remove(a);
}

TEST(the_same_file_under_another_spelling_is_still_the_same_file)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    std::string a = temp_pxa("tinysprite_doc_alias.pxa", 5);
    std::filesystem::path dir = std::filesystem::path(a).parent_path();
    std::filesystem::create_directories(dir / "tinysprite_alias_sub");
    std::string dotted =
        (dir / "tinysprite_alias_sub" / ".." / "tinysprite_doc_alias.pxa").string();
    std::string link = (dir / "tinysprite_alias_link.pxa").string();
    std::error_code ignored;
    std::filesystem::remove(link, ignored);
    std::filesystem::create_symlink(a, link, ignored);
    ASSERT_TRUE(t.open(a));
    ASSERT_EQ_INT(1, t.count());
    ASSERT_TRUE(t.open(dotted));
    ASSERT_EQ_INT(1, t.count()); /* shown, not opened a second time */
    if (!ignored) {
        ASSERT_TRUE(t.open(link));
        ASSERT_EQ_INT(1, t.count());
    }
    ASSERT_TRUE(t.addBlank(8, 8));
    ASSERT_TRUE(!t.saveAs(dotted));
    ASSERT_TRUE(t.error().find("open in another tab") != std::string::npos);
    std::filesystem::remove(link, ignored);
    std::filesystem::remove_all(dir / "tinysprite_alias_sub", ignored);
    std::filesystem::remove(a);
}

TEST(a_tab_cannot_be_closed_while_a_tool_is_being_dragged)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addBlank(8, 8));
    ASSERT_TRUE(t.pressed(1, 1));
    t.close(1); /* the active tab, mid-stroke */
    ASSERT_EQ_INT(2, t.count());
    t.close(0); /* another tab: asking about it would need the active one */
    ASSERT_EQ_INT(2, t.count());
    t.release();
    t.close(1);
    t.discard();
    ASSERT_EQ_INT(1, t.count());
}

TEST(quitting_in_the_middle_of_a_drag_still_quits)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.pressed(1, 1));
    t.quit();
    ASSERT_TRUE(!t.running());
}

TEST(switching_tabs_ends_a_middle_button_pick)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    t.editor().selectTool(ToolKind::Bucket);
    t.editor().tempPick(
        true); /* the middle button is held on the canvas: the eyedropper is borrowed */
    ASSERT_EQ_INT(static_cast<int>(ToolKind::Eyedropper), static_cast<int>(t.editor().tool()));
    ASSERT_TRUE(t.addBlank(8, 8));
    /* the tab left behind must have its own tool back: nothing else would ever end the pick there
     */
    ASSERT_EQ_INT(static_cast<int>(ToolKind::Bucket), static_cast<int>(t.editorOf(0).tool()));
    t.activate(0);
    ASSERT_EQ_INT(static_cast<int>(ToolKind::Bucket), static_cast<int>(t.editor().tool()));
}

/* A drop arrives from the window system whatever dialog is open. Opening it would switch the active
 * tab under the "unsaved changes" question, and Save would then save the wrong document. */
TEST(a_file_dropped_while_a_dialog_is_open_is_ignored)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "tinysprite_doc_drop";
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::string dropped = (dir / "dropped.pxa").string();
    pxa_save(Sprite(5, 5), dropped);
    std::string mine = (dir / "mine.pxa").string();
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    t.setPath(mine);
    t.dot(1, 1); /* "mine.pxa" has unsaved changes */
    ASSERT_TRUE(t.addBlank(6, 6));
    t.activate(0);
    t.close(0); /* asks Save / Discard / Cancel about it */
    ASSERT_TRUE(t.asking());
    t.drop(dropped);
    ASSERT_EQ_INT(2, t.count()); /* nothing was opened */
    ASSERT_EQ_INT(0, t.active());
    t.pressSave();
    bool written = fs::exists(mine);
    fs::remove_all(dir);
    ASSERT_TRUE(written);        /* Save saved the document the question was about */
    ASSERT_EQ_INT(1, t.count()); /* and then it was closed */
}

/* "Also write a .json file" is on by default, so the sheet dialog may overwrite a .json the user
 * never saw named: the overwrite question has to cover it. */
TEST(the_sheet_dialog_asks_before_overwriting_the_json_it_writes_beside_the_image)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "tinysprite_doc_sheetjson";
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::ofstream(dir / "hero.json") << "{\"mine\": true}";
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    std::string image = (dir / "hero.png").string();
    ASSERT_TRUE(!t.sheetOverwriteAllowed(image, true)); /* hero.png is new, but hero.json is not */
    ASSERT_TRUE(t.overwriteName() == "hero.json");
    ASSERT_TRUE(t.overwriteAllowedAgain(image));        /* asked once: the second OK goes through */
    ASSERT_TRUE(t.sheetOverwriteAllowed(image, false)); /* no JSON written: nothing to ask */
    fs::remove_all(dir);
}

int main()
{
    RUN(the_sheet_dialog_asks_before_overwriting_the_json_it_writes_beside_the_image);
    RUN(a_file_dropped_while_a_dialog_is_open_is_ignored);
    RUN(each_document_with_changes_gets_its_own_autosave_copy);
    RUN(recovered_copies_open_as_tabs_and_never_share_a_file);
    RUN(save_as_onto_the_file_of_another_tab_is_refused);
    RUN(the_same_file_under_another_spelling_is_still_the_same_file);
    RUN(a_tab_cannot_be_closed_while_a_tool_is_being_dragged);
    RUN(quitting_in_the_middle_of_a_drag_still_quits);
    RUN(switching_tabs_ends_a_middle_button_pick);
    RUN(a_closed_tabs_editor_is_kept_until_the_next_frame);
    RUN(a_split_needs_a_second_document_and_shows_it_beside_the_first);
    RUN(a_stacked_split_gives_both_panes_the_full_width);
    RUN(clicking_a_pane_makes_its_document_the_active_one);
    RUN(picking_a_tab_fills_the_active_pane_and_a_tab_in_the_other_pane_just_focuses_it);
    RUN(closing_a_tab_that_is_shown_in_a_pane_ends_the_split);
    RUN(closing_a_tab_that_is_not_shown_keeps_the_split);
    RUN(tabs_can_be_reordered_and_the_active_one_stays_the_same_document);
    RUN(the_app_starts_with_one_untitled_document);
    RUN(a_new_document_is_a_new_tab_and_is_shown);
    RUN(opening_a_file_reuses_an_untouched_untitled_tab_then_adds_tabs);
    RUN(an_edited_untitled_tab_is_kept_when_a_file_is_opened);
    RUN(each_tab_keeps_its_own_pixels_and_undo_history);
    RUN(the_tool_and_the_clipboard_follow_the_user_from_tab_to_tab);
    RUN(closing_a_clean_tab_removes_it_and_the_last_one_is_replaced_by_a_blank);
    RUN(closing_the_tabs_before_the_active_one_keeps_it_active);
    RUN(closing_a_tab_with_unsaved_changes_asks_first);
    RUN(quitting_asks_about_every_tab_with_unsaved_changes);
    RUN(quitting_with_nothing_unsaved_leaves_at_once);
    return TEST_SUMMARY();
}
