/* What the menu commands do and when they can run (app/commands.cpp): the ones that edit the
 * document wait for an open stroke to finish, the ones that depend on the document's state say so,
 * and the tab commands wrap around the open documents. */
#include <string>

#include "app.hpp"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_internal.h"
#include "minitest.hpp"

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
    int active() const
    {
        return app.active_;
    }
    bool enabled(CommandId id) const
    {
        return app.commandEnabled(id);
    }
    void run(CommandId id)
    {
        app.runCommand(id);
    }
    bool addTabs(int extra)
    {
        for (int i = 0; i < extra; i++) {
            if (!app.addBlankDocument(8, 8, false)) {
                return false;
            }
        }
        return true;
    }
    void activate(int index)
    {
        app.activateDocument(index);
    }
    /* One pencil dot: a whole stroke, so the history can undo it. */
    void dot(int x, int y)
    {
        editor().selectTool(ToolKind::Pencil);
        editor().press(x, y);
        editor().release();
    }
    /* A stroke that is still open, as while the mouse button is held. */
    void holdStroke()
    {
        editor().selectTool(ToolKind::Pencil);
        editor().press(2, 2);
    }
};

TEST(a_command_that_needs_history_waits_for_something_to_undo_or_redo)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(!t.enabled(CommandId::Undo));
    ASSERT_TRUE(!t.enabled(CommandId::Redo));
    t.dot(1, 1);
    ASSERT_TRUE(t.enabled(CommandId::Undo));
    ASSERT_TRUE(!t.enabled(CommandId::Redo));
    t.run(CommandId::Undo);
    ASSERT_TRUE(!t.enabled(CommandId::Undo));
    ASSERT_TRUE(t.enabled(CommandId::Redo));
}

TEST(the_selection_commands_need_a_selection)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(!t.enabled(CommandId::Cut));
    ASSERT_TRUE(!t.enabled(CommandId::Copy));
    t.run(CommandId::SelectAll);
    ASSERT_TRUE(t.enabled(CommandId::Cut));
    ASSERT_TRUE(t.enabled(CommandId::Copy));
}

TEST(the_layer_commands_follow_what_the_layers_allow)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(!t.enabled(CommandId::MergeLayerDown));
    t.run(CommandId::NewLayer);
    ASSERT_TRUE(t.enabled(CommandId::MergeLayerDown));
    ASSERT_TRUE(t.enabled(CommandId::MoveLayerDown));
    ASSERT_TRUE(!t.enabled(CommandId::MoveLayerUp)); /* the new layer is already on top */
}

TEST(an_open_stroke_holds_off_every_command_that_edits_but_not_the_view_or_save)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    t.dot(1, 1);
    t.run(CommandId::SelectAll);
    t.run(CommandId::NewLayer);
    ASSERT_TRUE(t.enabled(CommandId::Undo));
    ASSERT_TRUE(t.enabled(CommandId::Cut));
    ASSERT_TRUE(t.enabled(CommandId::MergeLayerDown));
    t.holdStroke();
    ASSERT_TRUE(!t.enabled(CommandId::Undo));
    ASSERT_TRUE(!t.enabled(CommandId::Cut));
    ASSERT_TRUE(!t.enabled(CommandId::Copy));
    ASSERT_TRUE(!t.enabled(CommandId::Paste));
    ASSERT_TRUE(!t.enabled(CommandId::SelectAll));
    ASSERT_TRUE(!t.enabled(CommandId::New));
    ASSERT_TRUE(!t.enabled(CommandId::Open));
    ASSERT_TRUE(!t.enabled(CommandId::NewLayer));
    ASSERT_TRUE(!t.enabled(CommandId::DuplicateLayer));
    ASSERT_TRUE(!t.enabled(CommandId::MergeLayerDown));
    ASSERT_TRUE(!t.enabled(CommandId::MoveLayerDown));
    ASSERT_TRUE(!t.enabled(CommandId::NewFrame));
    ASSERT_TRUE(!t.enabled(CommandId::DuplicateFrame));
    ASSERT_TRUE(!t.enabled(CommandId::CloseTab));
    ASSERT_TRUE(t.enabled(CommandId::Save));
    ASSERT_TRUE(t.enabled(CommandId::SaveAs));
    ASSERT_TRUE(t.enabled(CommandId::ExportImage));
    ASSERT_TRUE(t.enabled(CommandId::ZoomIn));
    ASSERT_TRUE(t.enabled(CommandId::ZoomOut));
    ASSERT_TRUE(t.enabled(CommandId::Fit));
    ASSERT_TRUE(t.enabled(CommandId::ToggleTimeline));
}

TEST(redo_is_held_off_by_an_open_stroke_too)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    t.dot(1, 1);
    t.run(CommandId::Undo);
    ASSERT_TRUE(t.enabled(CommandId::Redo));
    t.holdStroke();
    ASSERT_TRUE(!t.enabled(CommandId::Redo));
}

TEST(the_move_layer_commands_are_held_off_by_an_open_stroke_in_both_directions)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    t.run(CommandId::NewLayer);
    t.run(CommandId::NewLayer);
    t.editor().setActiveLayer(1); /* in the middle: it can go either way */
    ASSERT_TRUE(t.enabled(CommandId::MoveLayerUp));
    ASSERT_TRUE(t.enabled(CommandId::MoveLayerDown));
    t.holdStroke();
    ASSERT_TRUE(!t.enabled(CommandId::MoveLayerUp));
    ASSERT_TRUE(!t.enabled(CommandId::MoveLayerDown));
}

TEST(the_timeline_command_toggles_it_and_the_zoom_commands_step_the_view)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    const bool shown = t.editor().overlays().timeline;
    t.run(CommandId::ToggleTimeline);
    ASSERT_TRUE(t.editor().overlays().timeline != shown);
    t.run(CommandId::ToggleTimeline);
    ASSERT_TRUE(t.editor().overlays().timeline == shown);
    const float zoom = t.editor().view().viewport.zoom;
    t.run(CommandId::ZoomIn);
    ASSERT_TRUE(t.editor().view().viewport.zoom > zoom);
    t.run(CommandId::ZoomOut);
    ASSERT_TRUE(t.editor().view().viewport.zoom == zoom);
}

TEST(the_tab_commands_need_a_second_tab)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(!t.enabled(CommandId::NextTab));
    ASSERT_TRUE(!t.enabled(CommandId::PreviousTab));
    ASSERT_TRUE(t.addTabs(1));
    ASSERT_TRUE(t.enabled(CommandId::NextTab));
    ASSERT_TRUE(t.enabled(CommandId::PreviousTab));
    t.holdStroke();
    ASSERT_TRUE(!t.enabled(CommandId::NextTab));
    ASSERT_TRUE(!t.enabled(CommandId::PreviousTab));
}

TEST(the_tab_commands_move_one_tab_and_wrap_at_both_ends)
{
    AppTestAccess t;
    ASSERT_TRUE(t.start());
    ASSERT_TRUE(t.addTabs(2)); /* three tabs, the last one active */
    ASSERT_EQ_INT(2, t.active());
    t.run(CommandId::NextTab);
    ASSERT_EQ_INT(0, t.active());
    t.run(CommandId::NextTab);
    ASSERT_EQ_INT(1, t.active());
    t.run(CommandId::PreviousTab);
    ASSERT_EQ_INT(0, t.active());
    t.run(CommandId::PreviousTab);
    ASSERT_EQ_INT(2, t.active());
}

int main()
{
    RUN(a_command_that_needs_history_waits_for_something_to_undo_or_redo);
    RUN(the_selection_commands_need_a_selection);
    RUN(the_layer_commands_follow_what_the_layers_allow);
    RUN(an_open_stroke_holds_off_every_command_that_edits_but_not_the_view_or_save);
    RUN(redo_is_held_off_by_an_open_stroke_too);
    RUN(the_move_layer_commands_are_held_off_by_an_open_stroke_in_both_directions);
    RUN(the_timeline_command_toggles_it_and_the_zoom_commands_step_the_view);
    RUN(the_tab_commands_need_a_second_tab);
    RUN(the_tab_commands_move_one_tab_and_wrap_at_both_ends);
    return TEST_SUMMARY();
}
