/* The UI font is embedded (a subset of Geist, SIL OFL), so the text looks the same on every
 * machine and scales as vector outlines instead of stretching a 13 px bitmap. */
#include <cstring>

#include "imgui.h"
#include "minitest.hpp"
#include "theme.hpp"

TEST(load_fonts_adds_the_embedded_font_at_the_base_size)
{
    ImGui::CreateContext();
    ASSERT_TRUE(theme::load_fonts());
    ImGuiIO &io = ImGui::GetIO();
    ASSERT_EQ_INT(1, io.Fonts->Fonts.Size);
    ASSERT_TRUE(std::strstr(io.Fonts->Sources[0].Name, "Geist") != nullptr);
    /* The glyphs are rasterized without hinting, so each one is placed on a whole pixel and not
     * oversampled: that keeps the letter spacing and the stems even at 14 px. */
    ASSERT_TRUE(io.Fonts->Sources[0].PixelSnapH);
    ASSERT_EQ_INT(1, static_cast<int>(io.Fonts->Sources[0].OversampleH));
    ASSERT_EQ_INT(1, static_cast<int>(io.Fonts->Sources[0].OversampleV));
    theme::apply();
    ASSERT_TRUE(ImGui::GetStyle().FontSizeBase == theme::FONT_SIZE);
    ImGui::DestroyContext();
}

TEST(apply_keeps_the_font_size_across_a_scale_change)
{
    ImGui::CreateContext();
    ASSERT_TRUE(theme::load_fonts());
    theme::setScale(2.0f);
    theme::apply();
    theme::apply();
    ASSERT_TRUE(ImGui::GetStyle().FontSizeBase == theme::FONT_SIZE);
    ASSERT_TRUE(ImGui::GetStyle().FontScaleMain == 2.0f);
    theme::setScale(1.0f);
    ImGui::DestroyContext();
}

/* A control with no border is told apart from the panel by its fill alone, so the fill has to be
 * visibly lighter than the panel: a checkbox that vanishes into the background is unusable. */
static float luminance(const ImVec4 &c)
{
    return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
}

TEST(a_control_fill_is_visibly_lighter_than_the_panel)
{
    ImGui::CreateContext();
    theme::apply();
    const ImVec4 *c = ImGui::GetStyle().Colors;
    float panel = luminance(c[ImGuiCol_WindowBg]);
    ASSERT_TRUE(luminance(c[ImGuiCol_FrameBg]) - panel >= 0.02f);
    ASSERT_TRUE(luminance(c[ImGuiCol_Button]) - panel >= 0.02f);
    ImGui::DestroyContext();
}

/* A number is shown with its unit ("100 ms", "16 px") so it does not have to be guessed from the
 * label. The unit goes into an ImGui printf format, so a "%" in it must not be read as a
 * conversion. */
TEST(an_int_format_appends_the_unit_and_escapes_percent)
{
    ASSERT_TRUE(theme::int_format("") == "%d");
    ASSERT_TRUE(theme::int_format(" ms") == "%d ms");
    ASSERT_TRUE(theme::int_format(" px") == "%d px");
    ASSERT_TRUE(theme::int_format("%") == "%d%%");
    ASSERT_TRUE(theme::int_format("\xC3\x97") == "%d\xC3\x97"); /* the multiplication sign */
}

int main()
{
    RUN(load_fonts_adds_the_embedded_font_at_the_base_size);
    RUN(apply_keeps_the_font_size_across_a_scale_change);
    RUN(a_control_fill_is_visibly_lighter_than_the_panel);
    RUN(an_int_format_appends_the_unit_and_escapes_percent);
    return TEST_SUMMARY();
}
