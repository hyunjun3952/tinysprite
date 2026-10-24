/* The options bar has no scrollbar (a narrow window cuts the controls off), so it says when
 * something is hidden past its right end instead of leaving the user to guess. */
#include <memory>

#include "editor.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "minitest.hpp"
#include "theme.hpp"
#include "tools_panel.hpp"
#include "ui_harness.hpp"

namespace
{

/* The bar's cut-off flag after two frames at `width` (the first lays the content out). */
bool cutOffAt(float width)
{
    test_ui::Harness h;
    std::unique_ptr<Editor> editor = Editor::create(h.renderer, 16, 16);
    ToolsPanel tools(*editor);
    h.body = [&] { tools.contextBar(0.0f, 0.0f, width, 40.0f); };
    h.frame(900.0f, 650.0f, false);
    h.frame(900.0f, 650.0f, false);
    return tools.barCutOff();
}

} // namespace

TEST(a_narrow_options_bar_reports_that_controls_are_hidden)
{
    theme::setScale(1.0f);
    ASSERT_TRUE(cutOffAt(220.0f));
}

TEST(a_wide_options_bar_reports_nothing_hidden)
{
    theme::setScale(1.0f);
    ASSERT_TRUE(!cutOffAt(900.0f));
}

TEST(clicking_the_cut_mark_scrolls_the_bar_and_leaves_the_controls_under_it_alone)
{
    theme::setScale(1.0f);
    const float left = 300.0f; /* clear of the docked panels the harness draws */
    const float top = 300.0f;
    const float width = 220.0f;
    test_ui::Harness h;
    std::unique_ptr<Editor> editor = Editor::create(h.renderer, 16, 16);
    ToolsPanel tools(*editor);
    h.body = [&] { tools.contextBar(left, top, width, 40.0f); };
    h.frame(900.0f, 650.0f, false);
    h.frame(900.0f, 650.0f, false);
    ASSERT_TRUE(tools.barCutOff());
    const bool round = editor->options().brush_round;
    const bool perfect = editor->options().pixel_perfect;
    const float markX = left + width - theme::px(14.0f); /* the middle of the strip */
    const float markY = top + 20.0f;
    h.frame(markX, markY, false);
    h.frame(markX, markY, true);
    h.frame(markX, markY, false);
    h.frame(markX, markY, false);
    ImGuiWindow *bar = ImGui::FindWindowByName("##options_bar");
    ASSERT_TRUE(bar != nullptr);
    ASSERT_TRUE(bar->Scroll.x > 0.0f);
    ASSERT_TRUE(editor->options().brush_round == round);
    ASSERT_TRUE(editor->options().pixel_perfect == perfect);
}

int main()
{
    RUN(a_narrow_options_bar_reports_that_controls_are_hidden);
    RUN(a_wide_options_bar_reports_nothing_hidden);
    RUN(clicking_the_cut_mark_scrolls_the_bar_and_leaves_the_controls_under_it_alone);
    return TEST_SUMMARY();
}
