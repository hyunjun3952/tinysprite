/* Material Symbols glyphs carry different padding inside the same em box, so at one font size the
 * ink of the bucket is 17 px and the plus is 9 px. Each icon is drawn at the size that brings the
 * longer side of its ink to the same length, within limits, so the icons read as one set. */
#include <algorithm>

#include "icons.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "minitest.hpp"
#include "theme.hpp"
#include "ui_harness.hpp"

TEST(the_fit_brings_the_longer_side_to_the_target)
{
    ASSERT_TRUE(icons::ink_fit(13.0f, 11.0f, 15.0f) > 1.14f);
    ASSERT_TRUE(icons::ink_fit(13.0f, 11.0f, 15.0f) < 1.16f);
    ASSERT_TRUE(icons::ink_fit(15.0f, 17.0f, 15.0f) < 1.0f);
    ASSERT_TRUE(icons::ink_fit(15.0f, 15.0f, 15.0f) == 1.0f);
}

TEST(the_fit_is_limited_so_a_tiny_glyph_is_not_blown_up)
{
    ASSERT_TRUE(icons::ink_fit(9.0f, 2.0f, 15.0f) <= icons::INK_FIT_MAX);
    ASSERT_TRUE(icons::ink_fit(40.0f, 40.0f, 15.0f) >= icons::INK_FIT_MIN);
    ASSERT_TRUE(icons::ink_fit(0.0f, 0.0f, 15.0f) == 1.0f); /* no ink: leave the size alone */
}

TEST(every_glyph_icon_has_about_the_same_ink_length_at_every_scale)
{
    ImGui::CreateContext();
    ASSERT_TRUE(theme::load_fonts());
    ImFont *font = ImGui::GetIO().Fonts->Fonts[0];
    for (float scale : {1.0f, 1.5f, 2.0f, 3.0f}) {
        const float target = icons::INK_TARGET * scale;
        for (int i = 0; i < static_cast<int>(icons::Icon::Count); i++) {
            auto icon = static_cast<icons::Icon>(i);
            if (icons::is_drawn(icon)) {
                continue;
            }
            const float size = icons::fitted_glyph_size(font, icon, icons::glyph_size(scale));
            const ImFontGlyph *g =
                font->GetFontBaked(size)->FindGlyph(static_cast<ImWchar>(icons::codepoint(icon)));
            ASSERT_TRUE(g != nullptr);
            const float longest = std::max(g->X1 - g->X0, g->Y1 - g->Y0);
            ASSERT_TRUE(longest >= target - 2.0f * scale);
            ASSERT_TRUE(longest <= target + 2.0f * scale);
        }
    }
    ImGui::DestroyContext();
}

int main()
{
    RUN(the_fit_brings_the_longer_side_to_the_target);
    RUN(the_fit_is_limited_so_a_tiny_glyph_is_not_blown_up);
    RUN(every_glyph_icon_has_about_the_same_ink_length_at_every_scale);
    return TEST_SUMMARY();
}
