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
    ImVec2 docked = h.windowPos("Inspector");
    h.drag(docked.x + 40.0f, 8, 300, 150);
    ASSERT_TRUE(h.panels.floating(PanelId::Layers));
    ImVec2 pos = h.windowPos("Inspector");
    float width = ImGui::FindWindowByName("Inspector")->Size.x;
    /* The close button sits at the right end of the title bar. */
    h.drag(pos.x + width - 14.0f, pos.y + 8.0f, pos.x + width - 14.0f, pos.y + 8.0f);
    ASSERT_TRUE(!h.panels.visible(PanelId::Layers));
    h.frame(500, 300, false);
    h.frame(500, 300, false);
}

TEST(sections_are_visible_until_hidden)
{
    /* The Tools panel is the narrow tool bar (tools and shapes); Color, Layers and Properties are
     * sections of the right panel. */
    Panels panels;
    ASSERT_TRUE(panels.sectionVisible(PanelId::Layers, sections::COLOR));
    panels.setSectionVisible(PanelId::Layers, sections::COLOR, false);
    ASSERT_TRUE(!panels.sectionVisible(PanelId::Layers, sections::COLOR));
    ASSERT_TRUE(panels.sectionVisible(PanelId::Layers, sections::LAYERS));
    ASSERT_TRUE(panels.sectionVisible(PanelId::Tools, sections::SHAPES));
    panels.setSectionVisible(PanelId::Tools, sections::SHAPES, false);
    ASSERT_TRUE(!panels.sectionVisible(PanelId::Tools, sections::SHAPES));
    ASSERT_TRUE(panels.sectionVisible(PanelId::Tools, sections::TOOLS));
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

TEST(docked_panels_give_the_canvas_room_in_a_narrow_window)
{
    Harness h;
    h.frame(0.0f, 0.0f, false);
    float left = 0.0f;
    float right = 0.0f;
    h.panels.layout(left, right);
    SettingsMap saved;
    h.panels.store(saved);
    float chosen = 0.0f;
    settings_get_float(saved, "panel.layers.width", 0.0f, 10000.0f, chosen);
    ASSERT_TRUE(right == chosen); /* a 1000 px window at scale 1 squeezes nothing */
    theme::setScale(2.0f);
    h.panels.rescale(2.0f);
    h.panels.layout(left, right);
    h.panels.store(saved);
    settings_get_float(saved, "panel.layers.width", 0.0f, 10000.0f, chosen);
    theme::setScale(1.0f);
    /* at 2x the chosen inspector width would leave the canvas too little, so it is drawn narrower
     * (never below its minimum), while the saved width stays what the user chose */
    ASSERT_TRUE(right < chosen);
    ASSERT_TRUE(right >= 2.0f * 250.0f);
}

/* The width of a docked panel's window, after dragging its inner edge from (x, 300) to (toX, 300).
 */
static float width_after_edge_drag(Harness &h, const char *window, float x, float toX)
{
    h.frame(x, 300, false);
    h.frame(x, 300, false);
    h.drag(x, 300, toX, 300);
    h.frame(toX, 300, false);
    h.frame(toX, 300, false);
    return ImGui::FindWindowByName(window)->Size.x;
}

TEST(the_tool_bar_has_a_fixed_width)
{
    Harness h;
    h.frame(500, 300, false);
    h.frame(500, 300, false);
    float before = ImGui::FindWindowByName("Tools")->Size.x;
    /* its inner edge is at the window's right side; dragging it changes nothing */
    float after = width_after_edge_drag(h, "Tools", before - 1.0f, before + 200.0f);
    ASSERT_TRUE(after == before);
}

/* A floating tool bar has neither a close button nor a collapse arrow, so its title has the room
 * the docked one has. */
TEST(a_floating_tool_bar_has_no_close_button)
{
    Harness h;
    h.frame(500, 300, false);
    h.drag(40, 8, 100, 60);
    h.frame(500, 300, false);
    h.frame(500, 300, false);
    ASSERT_TRUE(h.panels.floating(PanelId::Tools));
    ASSERT_TRUE(!ImGui::FindWindowByName("Tools")->HasCloseButton);
    ASSERT_TRUE((ImGui::FindWindowByName("Tools")->Flags & ImGuiWindowFlags_NoCollapse) != 0);
}

/* A floating tool bar keeps its size too: dragging its bottom right corner changes nothing. */
TEST(a_floating_tool_bar_cannot_be_resized)
{
    Harness h;
    h.frame(500, 300, false);
    h.drag(40, 8, 100, 60);
    h.frame(500, 300, false);
    h.frame(500, 300, false);
    ASSERT_TRUE(h.panels.floating(PanelId::Tools));
    ImVec2 pos = h.windowPos("Tools");
    ImVec2 before = ImGui::FindWindowByName("Tools")->Size;
    float cornerX = pos.x + before.x - 2.0f;
    float cornerY = pos.y + before.y - 2.0f;
    h.frame(cornerX, cornerY, false);
    h.frame(cornerX, cornerY, false);
    h.drag(cornerX, cornerY, cornerX + 80.0f, cornerY + 30.0f);
    h.frame(cornerX + 80.0f, cornerY + 30.0f, false);
    ImVec2 after = ImGui::FindWindowByName("Tools")->Size;
    ASSERT_TRUE(after.x == before.x && after.y == before.y);
}

TEST(the_inspector_stays_between_its_minimum_and_maximum_width)
{
    Harness h;
    h.frame(500, 300, false);
    h.frame(500, 300, false);
    float start = ImGui::FindWindowByName("Inspector")->Size.x;
    float left = ImGui::FindWindowByName("Inspector")->Pos.x;
    /* its inner edge is its left side: dragging it right narrows it, but only down to the minimum
     * (below which the controls no longer fit) */
    float narrow = width_after_edge_drag(h, "Inspector", left + 1.0f, left + start);
    ASSERT_TRUE(narrow == 250.0f && narrow < start); /* the minimum */
    float left2 = ImGui::FindWindowByName("Inspector")->Pos.x;
    float wide = width_after_edge_drag(h, "Inspector", left2 + 1.0f, 0.0f);
    ASSERT_TRUE(wide <= 480.0f && wide > narrow);
}

/* Like the timeline's edge: two quick clicks on a panel's inner edge put it back at its default
 * width. */
TEST(double_clicking_a_panel_edge_restores_its_default_width)
{
    Harness h;
    h.frame(500, 300, false);
    h.frame(500, 300, false);
    const float start = ImGui::FindWindowByName("Inspector")->Size.x;
    float left = ImGui::FindWindowByName("Inspector")->Pos.x;
    const float narrow = width_after_edge_drag(h, "Inspector", left + 1.0f, left + 60.0f);
    ASSERT_TRUE(narrow < start);
    left = ImGui::FindWindowByName("Inspector")->Pos.x;
    h.frame(left + 1.0f, 300, false);
    h.frame(left + 1.0f, 300, false);
    for (int i = 0; i < 2; i++) {
        h.frame(left + 1.0f, 300, true);
        h.frame(left + 1.0f, 300, false);
    }
    h.frame(left + 1.0f, 300, false);
    h.frame(left + 1.0f, 300, false);
    ASSERT_TRUE(ImGui::FindWindowByName("Inspector")->Size.x == start);
}

TEST(the_right_panel_is_named_for_what_it_holds)
{
    Harness h;
    h.frame(0.0f, 0.0f, false);
    ASSERT_TRUE(ImGui::FindWindowByName("Inspector") != nullptr);
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
    RUN(docked_panels_give_the_canvas_room_in_a_narrow_window);
    RUN(closing_a_floating_panel_hides_it);
    RUN(the_tool_bar_has_a_fixed_width);
    RUN(a_floating_tool_bar_has_no_close_button);
    RUN(a_floating_tool_bar_cannot_be_resized);
    RUN(the_inspector_stays_between_its_minimum_and_maximum_width);
    RUN(double_clicking_a_panel_edge_restores_its_default_width);
    RUN(the_right_panel_is_named_for_what_it_holds);
    RUN(layers_panel_has_a_properties_section_that_can_be_hidden);
    return TEST_SUMMARY();
}
