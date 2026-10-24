/* The color block of the Inspector: what is below it must not move when the first recent color
 * appears (the row of recent colors used to be added only then, pushing the layers down). */
#include <memory>

#include "editor.hpp"
#include "imgui.h"
#include "minitest.hpp"
#include "tools_panel.hpp"
#include "ui_harness.hpp"

using test_ui::Harness;

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

int main()
{
    RUN(the_first_recent_color_does_not_move_what_is_below_the_color_block);
    return TEST_SUMMARY();
}
