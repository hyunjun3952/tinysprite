/* The icons are Material Symbols glyphs from the embedded font (Apache 2.0). Check the table is
 * well formed, that every glyph is really in the font, and that an icon is centered by the ink of
 * its glyph, not by the glyph's em box (which would sit most icons off to one side). */
#include <set>

#include "icons.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "minitest.hpp"
#include "theme.hpp"
#include "ui_harness.hpp"

/* The 14 tool and shape icons are bitmaps and the folder is drawn with lines (it would need a glyph
 * added to the font subset); they have no codepoint, every other icon is a glyph. */
TEST(the_tool_and_shape_bitmaps_and_the_folder_are_the_drawn_icons)
{
    int drawn = 0;
    for (int i = 0; i < static_cast<int>(icons::Icon::Count); i++) {
        drawn += icons::is_drawn(static_cast<icons::Icon>(i)) ? 1 : 0;
    }
    ASSERT_EQ_INT(15, drawn);
    ASSERT_TRUE(icons::is_drawn(icons::Icon::Diamond));
    ASSERT_TRUE(icons::is_drawn(icons::Icon::Folder));
    ASSERT_TRUE(icons::is_drawn(icons::Icon::Slice));
    ASSERT_TRUE(!icons::is_drawn(icons::Icon::Eye));
}

TEST(every_icon_has_its_own_private_use_codepoint_except_the_drawn_ones)
{
    std::set<unsigned> seen;
    for (int i = 0; i < static_cast<int>(icons::Icon::Count); i++) {
        auto icon = static_cast<icons::Icon>(i);
        unsigned cp = icons::codepoint(icon);
        if (icons::is_drawn(icon)) {
            ASSERT_EQ_INT(0, static_cast<int>(cp));
            continue;
        }
        ASSERT_TRUE(cp >= 0xE000 && cp <= 0xF8FF);
        ASSERT_TRUE(seen.insert(cp).second); /* two icons never share a glyph */
    }
}

TEST(every_icon_glyph_is_in_the_embedded_font)
{
    ImGui::CreateContext();
    ASSERT_TRUE(theme::load_fonts());
    ImFont *font = ImGui::GetIO().Fonts->Fonts[0];
    for (int i = 0; i < static_cast<int>(icons::Icon::Count); i++) {
        auto icon = static_cast<icons::Icon>(i);
        if (!icons::is_drawn(icon)) {
            ASSERT_TRUE(font->IsGlyphInFont(static_cast<ImWchar>(icons::codepoint(icon))));
        }
    }
    ImGui::DestroyContext();
}

TEST(a_glyph_is_centered_by_its_ink_on_whole_pixels)
{
    /* A 32 x 32 box, a glyph whose ink spans x 2..12 and y 4..16 from where its text starts. */
    ImVec2 origin = icons::glyph_origin(ImVec2(0, 0), ImVec2(32, 32), 2.0f, 4.0f, 12.0f, 16.0f);
    ASSERT_TRUE(origin.x == 9.0f); /* 16 - (2 + 12) / 2 */
    ASSERT_TRUE(origin.y == 6.0f); /* 16 - (4 + 16) / 2 */
    /* A box that is not at the window's origin, and an ink center that falls between pixels. */
    ImVec2 shifted = icons::glyph_origin(ImVec2(100, 50), ImVec2(133, 81), 0.0f, 0.0f, 11.0f, 9.0f);
    ASSERT_TRUE(shifted.x == 111.0f); /* floor(116.5 - 5.5) */
    ASSERT_TRUE(shifted.y == 61.0f);  /* floor(65.5 - 4.5) */
}

TEST(the_glyph_size_follows_the_ui_scale_in_whole_pixels)
{
    ASSERT_TRUE(icons::glyph_size(1.0f) == 20.0f);
    ASSERT_TRUE(icons::glyph_size(1.25f) == 25.0f);
    ASSERT_TRUE(icons::glyph_size(2.0f) == 40.0f);
    ASSERT_TRUE(icons::glyph_size(1.1f) == 22.0f);
}

/* A round button is smaller than a square one, so its glyph is drawn smaller too: a 20 px glyph
 * would crowd a circle of about 24 px. */
TEST(a_round_button_draws_its_glyph_smaller)
{
    ASSERT_TRUE(icons::round_glyph_size(1.0f) == 16.0f);
    ASSERT_TRUE(icons::round_glyph_size(2.0f) == 32.0f);
    ASSERT_TRUE(icons::round_glyph_size(1.5f) < icons::glyph_size(1.5f));
}

/* A disabled icon button shows the "not allowed" cursor and no tooltip; an enabled one explains
 * itself. (The layer panel used to cover this through its buttons.) */
TEST(a_disabled_icon_button_shows_a_blocked_cursor_and_no_tooltip)
{
    test_ui::Harness h;
    h.body = [] {
        ImGui::SetNextWindowPos(ImVec2(100, 100));
        ImGui::SetNextWindowSize(ImVec2(300, 80));
        ImGui::Begin("icons", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
        icons::button(icons::Icon::Trash, "##off", "Disabled tip", ImVec2(60, 40), false);
        ImGui::SameLine();
        icons::button(icons::Icon::Plus, "##on", "Enabled tip", ImVec2(60, 40), true);
        ImGui::End();
    };
    ImGui::GetStyle().HoverDelayShort = 0.0f;
    ImGui::GetStyle().HoverDelayNormal = 0.0f;
    ImGui::GetStyle().HoverStationaryDelay = 0.0f;
    auto hover = [&h](float x, float y, int &cursor, bool &tooltip) {
        for (int i = 0; i < 4; i++) {
            h.frame(x, y, false);
        }
        cursor = ImGui::GetMouseCursor();
        ImGuiWindow *tip = ImGui::FindWindowByName("##Tooltip_00");
        tooltip = tip != nullptr && tip->Active && !tip->Hidden;
    };
    int cursor = 0;
    bool tooltip = false;
    hover(130.0f, 120.0f, cursor, tooltip); /* the disabled one */
    ASSERT_TRUE(cursor == ImGuiMouseCursor_NotAllowed);
    ASSERT_TRUE(!tooltip);
    hover(200.0f, 120.0f, cursor, tooltip); /* the enabled one */
    ASSERT_TRUE(cursor != ImGuiMouseCursor_NotAllowed);
    ASSERT_TRUE(tooltip);
}

int main()
{
    RUN(the_tool_and_shape_bitmaps_and_the_folder_are_the_drawn_icons);
    RUN(every_icon_has_its_own_private_use_codepoint_except_the_drawn_ones);
    RUN(every_icon_glyph_is_in_the_embedded_font);
    RUN(a_glyph_is_centered_by_its_ink_on_whole_pixels);
    RUN(the_glyph_size_follows_the_ui_scale_in_whole_pixels);
    RUN(a_round_button_draws_its_glyph_smaller);
    RUN(a_disabled_icon_button_shows_a_blocked_cursor_and_no_tooltip);
    return TEST_SUMMARY();
}
