/* The options bar has no scrollbar (a narrow window cuts the controls off), so it says when
 * something is hidden past its right end instead of leaving the user to guess. */
#include <memory>

#include "editor.hpp"
#include "imgui.h"
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

int main()
{
    RUN(a_narrow_options_bar_reports_that_controls_are_hidden);
    RUN(a_wide_options_bar_reports_nothing_hidden);
    return TEST_SUMMARY();
}
