/* The color block of the Inspector: what is below it must not move when the first recent color
 * appears (the row of recent colors used to be added only then, pushing the layers down). */
#include <memory>

#include "editor.hpp"
#include "imgui.h"
#include "minitest.hpp"
#include "tools_panel.hpp"
#include "ui_harness.hpp"

using test_ui::Harness;

namespace
{

/* The y where the next item would go after the color block, in the Inspector's window. */
float below_color_block(Harness &h, ToolsPanel &tools)
{
    float y = 0.0f;
    h.layersBody = [&] {
        tools.colorBlock(h.panels);
        y = ImGui::GetCursorPosY();
    };
    h.frame(0.0f, 0.0f, false);
    h.frame(0.0f, 0.0f, false);
    return y;
}

} // namespace

TEST(the_first_recent_color_does_not_move_what_is_below_the_color_block)
{
    Harness h;
    std::unique_ptr<Editor> editor = Editor::create(h.renderer, 8, 8);
    ToolsPanel tools(*editor);
    const float empty = below_color_block(h, tools);
    ASSERT_TRUE(empty > 0.0f);
    editor->recordRecent(Rgba{255, 0, 0, 255});
    h.frame(0.0f, 0.0f, false);
    h.frame(0.0f, 0.0f, false);
    h.frame(0.0f, 0.0f, false);
    float after = 0.0f;
    h.layersBody = [&] {
        tools.colorBlock(h.panels);
        after = ImGui::GetCursorPosY();
    };
    h.frame(0.0f, 0.0f, false);
    h.frame(0.0f, 0.0f, false);
    ASSERT_TRUE(after == empty);
}

int main()
{
    RUN(the_first_recent_color_does_not_move_what_is_below_the_color_block);
    return TEST_SUMMARY();
}
