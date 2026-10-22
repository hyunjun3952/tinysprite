/* The icons are Material Symbols glyphs from the embedded font (Apache 2.0). Check the table is
 * well formed, that every glyph is really in the font, and that an icon is centered by the ink of
 * its glyph, not by the glyph's em box (which would sit most icons off to one side). */
#include <set>

#include "icons.hpp"
#include "imgui.h"
#include "minitest.hpp"
#include "theme.hpp"

/* Material Symbols has no plain rhombus (its "diamond" is a gem), so that one icon is drawn with
 * lines and has no codepoint; every other icon is a glyph. */
TEST(every_icon_has_its_own_private_use_codepoint_except_the_drawn_diamond)
{
    std::set<unsigned> seen;
    for (int i = 0; i < static_cast<int>(icons::Icon::Count); i++) {
        auto icon = static_cast<icons::Icon>(i);
        unsigned cp = icons::codepoint(icon);
        if (icon == icons::Icon::Diamond) {
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
        if (icon != icons::Icon::Diamond) {
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

int main()
{
    RUN(every_icon_has_its_own_private_use_codepoint_except_the_drawn_diamond);
    RUN(every_icon_glyph_is_in_the_embedded_font);
    RUN(a_glyph_is_centered_by_its_ink_on_whole_pixels);
    RUN(the_glyph_size_follows_the_ui_scale_in_whole_pixels);
    RUN(a_round_button_draws_its_glyph_smaller);
    return TEST_SUMMARY();
}
