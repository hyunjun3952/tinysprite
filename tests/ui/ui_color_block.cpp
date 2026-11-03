/* The color block of the Inspector: what is below it must not move when the first recent color
 * appears (the row of recent colors used to be added only then, pushing the layers down). */
#include <memory>

#include "editor.hpp"
#include "imgui.h"
#include "minitest.hpp"
#include "tools_panel.hpp"
#include "ui_harness.hpp"

using test_ui::Harness;

struct ToolsPanelTestAccess {
    static void askToReset(ToolsPanel &tools)
    {
        tools.openReset_ = true;
    }
};

TEST(the_first_recent_color_does_not_move_what_is_below_the_color_block)
{
    Harness h;
    std::unique_ptr<Editor> editor = Editor::create(h.renderer, 8, 8);
    ToolsPanel tools(*editor);
    float below = 0.0f; /* where the next item would go after the color block */
    h.layersBody = [&] {
        tools.colorBlock(h.panels);
        below = ImGui::GetCursorPosY();
    };
    h.frame(0.0f, 0.0f, false);
    h.frame(0.0f, 0.0f, false);
    const float empty = below;
    ASSERT_TRUE(empty > 0.0f);
    editor->recordRecent(Rgba{255, 0, 0, 255});
    h.frame(0.0f, 0.0f, false);
    h.frame(0.0f, 0.0f, false);
    ASSERT_TRUE(below == empty);
}

/* "Reset palette" follows the dialog rules (DESIGN.md, Dialogs): the danger button is the primary
 * on the right, Enter presses it, and Ctrl+Z brings the colors back (the dialog says so). */
TEST(enter_resets_the_palette_and_undo_brings_it_back)
{
    Harness h;
    std::unique_ptr<Editor> editor = Editor::create(h.renderer, 8, 8);
    ToolsPanel tools(*editor);
    h.layersBody = [&] { tools.colorBlock(h.panels); };
    editor->setForeground(Rgba{12, 34, 56, 255});
    editor->paletteAdd();
    ASSERT_TRUE(!editor->paletteIsDefault());
    ToolsPanelTestAccess::askToReset(tools);
    for (int i = 0; i < 4; i++) {
        h.frame(0.0f, 0.0f, false);
    }
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true);
    h.frame(0.0f, 0.0f, false);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false);
    h.frame(0.0f, 0.0f, false);
    ASSERT_TRUE(editor->paletteIsDefault());
    editor->undo();
    ASSERT_TRUE(!editor->paletteIsDefault());
}

int main()
{
    RUN(the_first_recent_color_does_not_move_what_is_below_the_color_block);
    RUN(enter_resets_the_palette_and_undo_brings_it_back);
    return TEST_SUMMARY();
}
