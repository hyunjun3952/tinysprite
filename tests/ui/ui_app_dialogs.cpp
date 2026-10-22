/* Whole frames of the app with synthetic key events: a key that closes a dialog must not also act
 * as a shortcut in the same frame. */
#include "app.hpp"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "imgui_internal.h"
#include "minitest.hpp"
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
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        app.buildUi();
        ImGui::Render();
    }
    Editor &editor()
    {
        return *app.editor_;
    }
    bool dirty() const
    {
        return app.dirty();
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

int main()
{
    RUN(escape_that_closes_the_unsaved_changes_dialog_keeps_the_selection);
    RUN(hovering_a_menu_title_opens_the_menu);
    RUN(the_layer_chords_add_duplicate_and_move_layers);
    RUN(double_clicking_the_timeline_edge_restores_its_default_height);
    RUN(dragging_the_timeline_edge_resizes_folds_and_unfolds_it);
    RUN(ctrl_t_hides_and_shows_the_timeline);
    return TEST_SUMMARY();
}
