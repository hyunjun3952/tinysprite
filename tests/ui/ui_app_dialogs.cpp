/* Whole frames of the app with synthetic key events: a key that closes a dialog must not also act
 * as a shortcut in the same frame. */
#include "app.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "imgui_internal.h"
#include "minitest.h"

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

int main()
{
    RUN(escape_that_closes_the_unsaved_changes_dialog_keeps_the_selection);
    return TEST_SUMMARY();
}
