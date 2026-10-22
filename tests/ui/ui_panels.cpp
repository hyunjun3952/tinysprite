/* Drives the side panels with synthetic mouse input: dragging a docked panel's title bar pulls it
 * out, and dragging it to a window edge docks it again. Runs under SDL_VIDEODRIVER=dummy. */
#include "minitest.hpp"
#include "theme.hpp"
#include "ui_harness.hpp"

using test_ui::Harness;

TEST(docked_panel_floats_when_its_title_bar_is_dragged)
{
    Harness h;
    h.frame(500, 300, false);
    ASSERT_TRUE(!h.panels.floating(PanelId::Tools));
    h.drag(40, 8, 400, 250);
    ASSERT_TRUE(h.panels.floating(PanelId::Tools));
    ASSERT_TRUE(!h.panels.floating(PanelId::Layers));
    /* It followed the mouse and kept its grip on the title bar. */
    ImVec2 pos = h.windowPos("Tools");
    ASSERT_TRUE(pos.x > 300.0f && pos.x < 400.0f);
    ASSERT_TRUE(pos.y > 200.0f && pos.y < 250.0f);
}

TEST(a_click_on_the_title_bar_does_not_float_the_panel)
{
    Harness h;
    h.frame(500, 300, false);
    h.drag(40, 8, 41, 8);
    ASSERT_TRUE(!h.panels.floating(PanelId::Tools));
}

TEST(dragging_a_floating_panel_to_an_edge_docks_it_again)
{
    Harness h;
    h.frame(500, 300, false);
    h.drag(40, 8, 400, 250);
    ASSERT_TRUE(h.panels.floating(PanelId::Tools));
    ImVec2 pos = h.windowPos("Tools");
    h.drag(pos.x + 30.0f, pos.y + 8.0f, 995.0f, 250.0f);
    ASSERT_TRUE(!h.panels.floating(PanelId::Tools));
    ASSERT_TRUE(h.windowPos("Tools").x > 400.0f); /* docked on the right, beside Layers */
}

TEST(releasing_away_from_an_edge_leaves_the_panel_floating)
{
    Harness h;
    h.frame(500, 300, false);
    h.drag(40, 8, 400, 250);
    h.frame(450, 300, false);
    ASSERT_TRUE(h.panels.floating(PanelId::Tools));
}

TEST(a_hidden_panel_can_be_skipped_for_many_frames)
{
    Harness h;
    h.frame(500, 300, false);
    h.panels.setVisible(PanelId::Tools, false);
    h.frame(500, 300, false);
    h.frame(500, 300, false);
    h.panels.setVisible(PanelId::Layers, false);
    h.frame(500, 300, false);
    ASSERT_TRUE(!h.panels.visible(PanelId::Tools));
    h.panels.setVisible(PanelId::Tools, true);
    h.frame(500, 300, false);
    ASSERT_TRUE(h.panels.visible(PanelId::Tools));
}

TEST(closing_a_floating_panel_hides_it)
{
    Harness h;
    h.frame(500, 300, false);
    h.drag(40, 8, 400, 250);
    ImVec2 pos = h.windowPos("Tools");
    float width = ImGui::FindWindowByName("Tools")->Size.x;
    /* The close button sits at the right end of the title bar. */
    h.drag(pos.x + width - 14.0f, pos.y + 8.0f, pos.x + width - 14.0f, pos.y + 8.0f);
    ASSERT_TRUE(!h.panels.visible(PanelId::Tools));
    h.frame(500, 300, false);
    h.frame(500, 300, false);
}

TEST(sections_are_visible_until_hidden)
{
    Panels panels;
    ASSERT_TRUE(panels.sectionVisible(PanelId::Tools, sections::RECENT));
    panels.setSectionVisible(PanelId::Tools, sections::RECENT, false);
    ASSERT_TRUE(!panels.sectionVisible(PanelId::Tools, sections::RECENT));
    ASSERT_TRUE(panels.sectionVisible(PanelId::Tools, sections::COLOR));
    /* The Layers panel has its own sections. */
    ASSERT_TRUE(panels.sectionVisible(PanelId::Layers, sections::LAYERS));
    panels.setSectionVisible(PanelId::Layers, sections::RECENT, false);
    ASSERT_TRUE(!panels.sectionVisible(PanelId::Tools, sections::RECENT));
}

TEST(layers_panel_has_a_properties_section_that_can_be_hidden)
{
    Panels panels;
    ASSERT_TRUE(panels.sectionVisible(PanelId::Layers, sections::PROPERTIES));
    panels.setSectionVisible(PanelId::Layers, sections::PROPERTIES, false);
    ASSERT_TRUE(!panels.sectionVisible(PanelId::Layers, sections::PROPERTIES));
    ASSERT_TRUE(panels.sectionVisible(PanelId::Layers, sections::LAYERS));
}

TEST(a_name_that_is_not_a_section_counts_as_visible)
{
    Panels panels;
    panels.setSectionVisible(PanelId::Layers, "nothing", false);
    ASSERT_TRUE(panels.sectionVisible(PanelId::Layers, "nothing"));
    ASSERT_TRUE(panels.sectionVisible(PanelId::Layers, sections::LAYERS));
}

TEST(reset_shows_every_section_again)
{
    Panels panels;
    panels.setSectionVisible(PanelId::Tools, sections::TOOLS, false);
    panels.setSectionVisible(PanelId::Layers, sections::LAYERS, false);
    panels.reset();
    ASSERT_TRUE(panels.sectionVisible(PanelId::Tools, sections::TOOLS));
    ASSERT_TRUE(panels.sectionVisible(PanelId::Layers, sections::LAYERS));
}

TEST(reset_keeps_the_widths_at_the_current_ui_scale)
{
    Harness h;
    h.frame(0.0f, 0.0f, false);
    float before = 0.0f;
    float right = 0.0f;
    h.panels.layout(before, right);
    theme::setScale(2.0f);
    h.panels.rescale(2.0f);
    float scaled = 0.0f;
    h.panels.layout(scaled, right);
    h.panels.reset();
    float afterReset = 0.0f;
    h.panels.layout(afterReset, right);
    theme::setScale(1.0f);
    ASSERT_TRUE(scaled > before);
    ASSERT_TRUE(afterReset == scaled);
}

TEST(symmetry_section_is_hidden_until_switched_on)
{
    Harness h;
    ASSERT_TRUE(!h.panels.sectionVisible(PanelId::Tools, sections::SYMMETRY));
    ASSERT_TRUE(h.panels.sectionVisible(PanelId::Tools, sections::SHAPES));
    h.panels.setSectionVisible(PanelId::Tools, sections::SYMMETRY, true);
    ASSERT_TRUE(h.panels.sectionVisible(PanelId::Tools, sections::SYMMETRY));
}

int main()
{
    RUN(docked_panel_floats_when_its_title_bar_is_dragged);
    RUN(a_click_on_the_title_bar_does_not_float_the_panel);
    RUN(dragging_a_floating_panel_to_an_edge_docks_it_again);
    RUN(releasing_away_from_an_edge_leaves_the_panel_floating);
    RUN(a_hidden_panel_can_be_skipped_for_many_frames);
    RUN(sections_are_visible_until_hidden);
    RUN(a_name_that_is_not_a_section_counts_as_visible);
    RUN(reset_shows_every_section_again);
    RUN(reset_keeps_the_widths_at_the_current_ui_scale);
    RUN(closing_a_floating_panel_hides_it);
    RUN(symmetry_section_is_hidden_until_switched_on);
    RUN(layers_panel_has_a_properties_section_that_can_be_hidden);
    return TEST_SUMMARY();
}
