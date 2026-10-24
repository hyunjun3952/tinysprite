/* Whole frames of the app with synthetic key events: a key that closes a dialog must not also act
 * as a shortcut in the same frame. */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "app.hpp"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_internal.h"
#include "minitest.hpp"
#include "theme.hpp"
#include "timeline_panel.hpp"

struct AppTestAccess {
    App app;

    bool start()
    {
        AppOptions options;
        options.width = 16;
        options.height = 16;
        options.maxFrames = 1; /* no settings or autosave of the user's */
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
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        app.buildUi();
        ImGui::Render();
    }
    Editor &editor()
    {
        return *app.editor_;
    }
    SDL_Window *window()
    {
        return app.window_;
    }
    bool dirty() const
    {
        return app.dirty();
    }
    void openFilter(FilterParams::Kind kind)
    {
        app.openFilter(kind);
    }
    bool runFilter()
    {
        return app.runFilter();
    }
    void setFilterColors(const float *from, const float *to)
    {
        std::copy(from, from + 4, app.filterColors_[1]);
        std::copy(to, to + 4, app.filterColors_[2]);
    }
    void askBeforeLeaving()
    {
        app.guardUnsaved([] {});
    }
    /* The sash on the timeline's top edge (or on the status bar when it is folded away). */
    ImVec2 sash()
    {
        const ImGuiWindow *w = ImGui::FindWindowByName("##timeline_sash");
        return w ? ImVec2(w->Pos.x + w->Size.x / 2.0f, w->Pos.y + w->Size.y / 2.0f)
                 : ImVec2(-1.0f, -1.0f);
    }
    /* Grab the sash and move the mouse by `dy` pixels, then let go. */
    void dragSash(float dy)
    {
        ImVec2 from = sash();
        ImGuiIO &io = ImGui::GetIO();
        io.AddMousePosEvent(from.x, from.y);
        frame();
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        for (int i = 1; i <= 8; i++) {
            io.AddMousePosEvent(from.x, from.y + dy * static_cast<float>(i) / 8.0f);
            frame();
        }
        io.AddMouseButtonEvent(0, false);
        frame();
        frame();
    }
    /* Two quick clicks on the sash, without moving the mouse. */
    void doubleClickSash()
    {
        ImVec2 at = sash();
        ImGuiIO &io = ImGui::GetIO();
        io.MouseSingleClickDelay = 20.0f; /* the test frames are slow under the sanitizers: */
        io.MouseDoubleClickTime = 10.0f;  /* give the second click all the time it needs */
        io.AddMousePosEvent(at.x, at.y);
        frame();
        frame();
        for (int i = 0; i < 2; i++) {
            io.AddMouseButtonEvent(0, true);
            frame();
            io.AddMouseButtonEvent(0, false);
            frame();
        }
        frame();
    }
    /* Move the mouse to (x, y) and let a few frames pass. */
    void hover(float x, float y)
    {
        ImGui::GetIO().AddMousePosEvent(x, y);
        for (int i = 0; i < 4; i++) {
            frame();
        }
    }
    static int openPopups()
    {
        return ImGui::GetCurrentContext()->OpenPopupStack.Size;
    }
    /* Ctrl (+ Shift) and one key, held for a frame. */
    void pressChord(ImGuiKey key, bool shift)
    {
        ImGuiIO &io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiMod_Shift, shift);
        io.AddKeyEvent(key, true);
        frame();
        io.AddKeyEvent(key, false);
        io.AddKeyEvent(ImGuiMod_Shift, false);
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
        frame();
        frame();
    }
    void pressCtrlT()
    {
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
        ImGui::GetIO().AddKeyEvent(ImGuiKey_T, true);
        frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_T, false);
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
        frame();
        frame();
    }
    /* The path field of the file dialog. */
    std::string path() const
    {
        return app.pathInput_;
    }
    /* Opens "Save as" showing the folder list with `typed` in the path field. */
    void openSaveDialog(const std::string &typed)
    {
        app.askForPath(App::FileAction::SaveAs);
        std::snprintf(app.pathInput_, sizeof(app.pathInput_), "%s", typed.c_str());
        app.browserOpen_ = true;
        for (int i = 0; i < 4; i++) {
            frame();
        }
    }
    /* The middle of row `row` of the dialog's folder tree ("../" is row 0), `x` pixels from its
     * left edge: a name is clicked well right of the arrows (a nested node's arrow is further in),
     * an arrow at 20. */
    ImVec2 listRow(int row, float x = 120.0f) const
    {
        for (const ImGuiWindow *w : ImGui::GetCurrentContext()->Windows) {
            if (std::strstr(w->Name, "##files") != nullptr) {
                /* a tree row: the text and its padding above and below, and the gap after it */
                float pitch = ImGui::GetFontSize() + 2.0f * theme::px(App::TREE_ROW_PADDING) +
                              ImGui::GetStyle().ItemSpacing.y;
                return {w->InnerRect.Min.x + x, w->InnerRect.Min.y + w->WindowPadding.y +
                                                    (static_cast<float>(row) + 0.5f) * pitch};
            }
        }
        return {-1.0f, -1.0f};
    }
    /* How many rows of the tree are shown (not counting "../"): its content height over a row. */
    int treeRows() const
    {
        for (const ImGuiWindow *w : ImGui::GetCurrentContext()->Windows) {
            if (std::strstr(w->Name, "##files") != nullptr) {
                float pitch = ImGui::GetFontSize() + 2.0f * theme::px(App::TREE_ROW_PADDING) +
                              ImGui::GetStyle().ItemSpacing.y;
                /* n rows are n heights and n - 1 gaps tall */
                float rows = (w->ContentSize.y + ImGui::GetStyle().ItemSpacing.y) / pitch;
                return static_cast<int>(std::lround(rows)) - 1;
            }
        }
        return -1;
    }
    void expandAll()
    {
        app.requestOpenAll(1);
        for (int i = 0; i < 4; i++) {
            frame();
        }
    }
    void collapseAll()
    {
        app.requestOpenAll(-1);
        for (int i = 0; i < 4; i++) {
            frame();
        }
    }
    /* One click (press and release) at `at`; frames in between. */
    void click(ImVec2 at)
    {
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
    void pressEscape()
    {
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
        frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
        frame();
        frame();
    }
};

TEST(escape_that_closes_the_unsaved_changes_dialog_keeps_the_selection)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    Editor &editor = t.editor();
    editor.selectAll();
    editor.setFrameDuration(0, 250); /* an edit: the document is dirty */
    ASSERT_TRUE(t.dirty());
    t.askBeforeLeaving();
    for (int i = 0; i < 3; i++) {
        t.frame();
    }
    ASSERT_TRUE(ImGui::GetCurrentContext()->OpenPopupStack.Size > 0);
    t.pressEscape();
    ASSERT_EQ_INT(0, ImGui::GetCurrentContext()->OpenPopupStack.Size);
    ASSERT_TRUE(editor.hasSelection());
}

TEST(the_filter_dialog_opens_and_escape_closes_it)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    t.openFilter(FilterParams::Kind::HueBrightness);
    for (int i = 0; i < 3; i++) {
        t.frame();
    }
    ASSERT_TRUE(ImGui::GetCurrentContext()->OpenPopupStack.Size > 0);
    t.pressEscape();
    ASSERT_EQ_INT(0, ImGui::GetCurrentContext()->OpenPopupStack.Size);
}

TEST(applying_the_filter_dialog_recolors_the_cel_in_one_undo_step)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    Editor &editor = t.editor();
    editor.activeCel()->set(0, 0, Rgba{255, 0, 0, 255});
    const float red[4] = {1, 0, 0, 1};
    const float blue[4] = {0, 0, 1, 1};
    t.openFilter(FilterParams::Kind::ReplaceColor);
    t.setFilterColors(red, blue);
    ASSERT_TRUE(t.runFilter());
    ASSERT_TRUE(editor.activeCel()->get(0, 0) == Rgba({0, 0, 255, 255}));
    editor.undo();
    ASSERT_TRUE(editor.activeCel()->get(0, 0) == Rgba({255, 0, 0, 255}));
}

TEST(enter_in_the_hex_field_of_the_filter_dialog_only_commits_the_color)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    Editor &editor = t.editor();
    editor.activeCel()->set(0, 0, Rgba{255, 0, 0, 255});
    const float red[4] = {1, 0, 0, 1};
    const float blue[4] = {0, 0, 1, 1};
    t.openFilter(FilterParams::Kind::ReplaceColor);
    t.setFilterColors(red, blue);
    for (int i = 0; i < 4; i++) {
        t.frame();
    }
    ImGuiWindow *w = ImGui::FindWindowByName("Filter");
    ASSERT_TRUE(w != nullptr);
    ImGuiID hex =
        ImGui::GetIDWithSeed("##hex", nullptr, ImGui::GetIDWithSeed("from_hex", nullptr, w->ID));
    ImGuiIO &io = ImGui::GetIO();
    bool found = false;
    for (float y = w->Pos.y; y < w->Pos.y + w->Size.y && !found; y += 3.0f) {
        io.AddMousePosEvent(w->Pos.x + w->Size.x * 0.6f, y);
        t.frame();
        io.AddMouseButtonEvent(0, true);
        t.frame();
        io.AddMouseButtonEvent(0, false);
        t.frame();
        found = ImGui::GetCurrentContext()->ActiveId == hex;
    }
    ASSERT_TRUE(found);
    io.AddKeyEvent(ImGuiKey_Enter, true);
    t.frame();
    io.AddKeyEvent(ImGuiKey_Enter, false);
    t.frame();
    t.frame();
    /* the dialog should still be open and nothing recolored: the user has not pressed Apply */
    ASSERT_TRUE(ImGui::GetCurrentContext()->OpenPopupStack.Size > 0);
    ASSERT_TRUE(editor.activeCel()->get(0, 0) == Rgba({255, 0, 0, 255}));
}

TEST(the_tilemap_window_draws_a_map_and_follows_undo_and_tab_changes)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    Editor &editor = t.editor();
    editor.activeCel()->set(0, 0, Rgba{255, 0, 0, 255});
    ASSERT_TRUE(editor.addTilesetFromCel(4, 4));
    ASSERT_TRUE(editor.addTilemap(0, 3, 3));
    editor.overlays().tilemap_window = true;
    for (int i = 0; i < 4; i++) {
        t.frame();
    }
    ASSERT_TRUE(editor.paintTile(1, 1, 1));
    ASSERT_TRUE(editor.endTilePaint());
    for (int i = 0; i < 2; i++) {
        t.frame();
    }
    editor.undo();
    editor.undo(); /* the map goes: the window falls back to its hint */
    for (int i = 0; i < 2; i++) {
        t.frame();
    }
    ASSERT_EQ_INT(-1, editor.selectedTilemap());
    editor.overlays().tilemap_window = false;
    t.frame();
}

/* Tab as the window system delivers it: a key event the app reads before ImGui does. */
static void push_tab(SDL_Window *window, bool down)
{
    SDL_Event event = {};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = SDL_SCANCODE_TAB;
    event.key.key = SDLK_TAB;
    event.key.down = down;
    event.key.windowID = SDL_GetWindowID(window); /* the backend ignores a key for no window */
    SDL_PushEvent(&event);
}

/* A keyboard-only user has to be able to get into the UI: Tab, with no mouse click before it, puts
 * the focus (and its ring) on a control. The canvas, which has the focus at the start, has nothing
 * to tab to. */
TEST(tab_enters_the_ui_without_a_mouse_click)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    const ImGuiContext &g = *ImGui::GetCurrentContext();
    ASSERT_TRUE(g.NavId == 0);
    push_tab(t.window(), true);
    t.frame();
    push_tab(t.window(), false);
    t.frame();
    t.frame();
    ASSERT_TRUE(g.NavId != 0);
    ASSERT_TRUE(ImGui::GetIO().NavVisible);
    ASSERT_TRUE(g.NavWindow != nullptr && std::strcmp(g.NavWindow->Name, "Tools") == 0);
}
TEST(ctrl_t_hides_and_shows_the_timeline)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.editor().overlays().timeline);
    t.pressCtrlT();
    ASSERT_TRUE(!t.editor().overlays().timeline);
    t.pressCtrlT();
    ASSERT_TRUE(t.editor().overlays().timeline);
}

TEST(dragging_the_timeline_edge_resizes_folds_and_unfolds_it)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    Editor::Overlays &o = t.editor().overlays();
    ASSERT_TRUE(o.timeline);
    const float before = o.timeline_height;
    t.dragSash(-60.0f); /* up: taller */
    ASSERT_TRUE(o.timeline && o.timeline_height > before + 40.0f);
    const float tall = o.timeline_height;
    t.dragSash(900.0f); /* far down: folds away, remembering its height */
    ASSERT_TRUE(!o.timeline);
    ASSERT_TRUE(o.timeline_height == tall);
    t.dragSash(-150.0f); /* up from the status bar: comes back at the height dragged to */
    ASSERT_TRUE(o.timeline);
    ASSERT_TRUE(o.timeline_height > 100.0f && o.timeline_height < tall);
}

TEST(double_clicking_the_timeline_edge_restores_its_default_height)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    Editor::Overlays &o = t.editor().overlays();
    t.dragSash(-90.0f);
    ASSERT_TRUE(o.timeline_height > TimelinePanel::BASE_HEIGHT + 60.0f);
    t.doubleClickSash();
    ASSERT_TRUE(o.timeline && o.timeline_height == TimelinePanel::BASE_HEIGHT);
    /* and from a folded timeline it brings it back at that height */
    t.dragSash(900.0f);
    ASSERT_TRUE(!o.timeline);
    t.doubleClickSash();
    ASSERT_TRUE(o.timeline && o.timeline_height == TimelinePanel::BASE_HEIGHT);
}

TEST(the_layer_chords_add_duplicate_and_move_layers)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    Editor &editor = t.editor();
    ASSERT_EQ_INT(1, editor.sprite().layerCount());
    t.pressChord(ImGuiKey_N, true); /* Ctrl+Shift+N */
    ASSERT_EQ_INT(2, editor.sprite().layerCount());
    t.pressChord(ImGuiKey_D, true); /* Ctrl+Shift+D */
    ASSERT_EQ_INT(3, editor.sprite().layerCount());
    const int active = editor.activeLayer();
    t.pressChord(ImGuiKey_LeftBracket, false); /* Ctrl+[ : down one place */
    ASSERT_EQ_INT(active - 1, editor.activeLayer());
    t.pressChord(ImGuiKey_RightBracket, false); /* Ctrl+] : and back up */
    ASSERT_EQ_INT(active, editor.activeLayer());
    editor.undo();
    editor.undo();
    editor.undo();
    ASSERT_EQ_INT(2, editor.sprite().layerCount()); /* the duplicate went; then the two moves */
}

/* A menu of the menu bar opens when the mouse rests on its title, without a click, and the next
 * title the mouse crosses takes over. */
TEST(hovering_a_menu_title_opens_the_menu)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    t.hover(300.0f, 300.0f); /* away from the bar */
    ASSERT_EQ_INT(0, AppTestAccess::openPopups());
    t.hover(20.0f, 10.0f); /* the File title */
    ASSERT_TRUE(AppTestAccess::openPopups() > 0);
    if (AppTestAccess::openPopups() == 0) {
        return;
    }
    ImGuiID file = ImGui::GetCurrentContext()->OpenPopupStack[0].PopupId;
    t.hover(58.0f, 10.0f); /* the Edit title: another menu */
    ASSERT_TRUE(AppTestAccess::openPopups() > 0);
    if (AppTestAccess::openPopups() > 0) {
        ASSERT_TRUE(ImGui::GetCurrentContext()->OpenPopupStack[0].PopupId != file);
    }
}

/* The dialog joins path parts with "/" while std::filesystem on Windows writes "\\": compare the
 * paths with one separator. */
static std::string slashes(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

/* A folder's name in the file dialog's tree picks it as the place to save to: its path goes in
 * front of the file name, the tree keeps its root, and the arrow opens the folder to show what is
 * in it. */
TEST(clicking_a_folder_of_the_file_dialog_puts_it_before_the_file_name)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "tinysprite_dialog_test";
    fs::remove_all(root);
    fs::create_directories(root / "alpha" / "inner");
    fs::create_directories(root / "beta");
    std::ofstream(root / "alpha" / "note.pxa") << "x";
    t.openSaveDialog((root / "sprite.pxa").string());
    ASSERT_TRUE(t.listRow(0).x > 0.0f);
    t.click(t.listRow(1)); /* alpha/ */
    ASSERT_TRUE(slashes(t.path()) == slashes((root / "alpha" / "sprite.pxa").string()));
    t.click(t.listRow(1)); /* the same folder again: nothing is stacked onto the path */
    ASSERT_TRUE(slashes(t.path()) == slashes((root / "alpha" / "sprite.pxa").string()));
    t.click(t.listRow(2)); /* beta/: the tree still starts at root, so the rows did not move */
    ASSERT_TRUE(slashes(t.path()) == slashes((root / "beta" / "sprite.pxa").string()));
    t.click(t.listRow(1, 20.0f)); /* the arrow of alpha/ opens it: inner/ and note.pxa show */
    t.click(t.listRow(2));        /* inner/ */
    ASSERT_TRUE(slashes(t.path()) == slashes((root / "alpha" / "inner" / "sprite.pxa").string()));
    t.click(t.listRow(3)); /* note.pxa: a file replaces the name */
    ASSERT_TRUE(slashes(t.path()) == slashes((root / "alpha" / "note.pxa").string()));
    t.click(t.listRow(0)); /* ../ : the tree starts one level up, the file name stays as it was */
    ASSERT_TRUE(slashes(t.path()) == slashes((root.parent_path() / "note.pxa").string()));
    std::error_code ignored;
    fs::remove_all(root, ignored);
}

/* "Expand all" opens every folder (to a depth), "Collapse all" closes them again. */
TEST(expand_all_and_collapse_all_open_and_close_the_whole_tree)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "tinysprite_dialog_open_all";
    fs::remove_all(root);
    fs::create_directories(root / "alpha" / "inner" / "deepest");
    fs::create_directories(root / "beta");
    std::ofstream(root / "alpha" / "inner" / "deepest" / "x.pxa") << "x";
    std::ofstream(root / "beta" / "y.pxa") << "x"; /* no folder is empty: that has its own row */
    t.openSaveDialog((root / "sprite.pxa").string());
    ASSERT_EQ_INT(2, t.treeRows()); /* alpha/ and beta/ are closed: ../ is not counted */
    t.expandAll();
    ASSERT_EQ_INT(6, t.treeRows()); /* alpha/ inner/ deepest/ x.pxa beta/ y.pxa */
    t.collapseAll();
    ASSERT_EQ_INT(2, t.treeRows());
    std::error_code ignored;
    fs::remove_all(root, ignored);
}

/* Only the rows in view are drawn: a folder with tens of thousands of files keeps the dialog
 * responsive (it took 100 ms a frame with the sanitizers before, 20x the bound below). */
TEST(a_folder_with_very_many_files_does_not_slow_the_file_dialog_down)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / "tinysprite_dialog_many";
    fs::remove_all(root);
    fs::create_directories(root);
    const int files = 20000;
    for (int i = 0; i < files; i++) {
        std::ofstream(root / (std::to_string(i) + ".pxa"));
    }
    t.openSaveDialog((root / "sprite.pxa").string());
    ASSERT_EQ_INT(files, t.treeRows()); /* the rows that are not drawn still take their room */
    const auto start = std::chrono::steady_clock::now();
    const int frames = 5;
    for (int i = 0; i < frames; i++) {
        t.frame();
    }
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
            .count() /
        frames;
    std::printf("  %d files: %.1f ms per frame\n", files, ms);
    ASSERT_TRUE(ms < 25.0);
    std::error_code ignored;
    fs::remove_all(root, ignored);
}

int main()
{
    RUN(expand_all_and_collapse_all_open_and_close_the_whole_tree);
    RUN(clicking_a_folder_of_the_file_dialog_puts_it_before_the_file_name);
    RUN(escape_that_closes_the_unsaved_changes_dialog_keeps_the_selection);
    RUN(the_filter_dialog_opens_and_escape_closes_it);
    RUN(the_tilemap_window_draws_a_map_and_follows_undo_and_tab_changes);
    RUN(applying_the_filter_dialog_recolors_the_cel_in_one_undo_step);
    RUN(enter_in_the_hex_field_of_the_filter_dialog_only_commits_the_color);
    RUN(hovering_a_menu_title_opens_the_menu);
    RUN(the_layer_chords_add_duplicate_and_move_layers);
    RUN(double_clicking_the_timeline_edge_restores_its_default_height);
    RUN(dragging_the_timeline_edge_resizes_folds_and_unfolds_it);
    RUN(tab_enters_the_ui_without_a_mouse_click);
    RUN(ctrl_t_hides_and_shows_the_timeline);
    RUN(a_folder_with_very_many_files_does_not_slow_the_file_dialog_down);
    return TEST_SUMMARY();
}
