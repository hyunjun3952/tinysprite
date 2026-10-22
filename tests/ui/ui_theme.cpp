/* The UI font is embedded (a subset of Geist, SIL OFL), so the text looks the same on every
 * machine and scales as vector outlines instead of stretching a 13 px bitmap. */
#include <algorithm>
#include <cmath>
#include <cstring>

#include "imgui.h"
#include "minitest.hpp"
#include "theme.hpp"

namespace
{

double luminance(unsigned rgb)
{
    double sum = 0.0;
    const double weight[3] = {0.2126, 0.7152, 0.0722};
    for (int i = 0; i < 3; i++) {
        double c = static_cast<double>((rgb >> (16 - 8 * i)) & 0xff) / 255.0;
        sum += weight[i] * (c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4));
    }
    return sum;
}

double contrast(unsigned a, unsigned b)
{
    double la = luminance(a);
    double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

} // namespace

TEST(text_on_a_filled_dialog_button_reaches_the_4_5_contrast_ratio)
{
    ASSERT_TRUE(contrast(theme::INK, theme::DANGER) >= 4.5);
    ASSERT_TRUE(contrast(theme::INK, theme::DANGER_STRONG) >= 4.5);
    ASSERT_TRUE(contrast(theme::ACCENT_TEXT, theme::ACCENT) >= 4.5);
}

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
    /* The same inside the menus, the color picker and the dialogs, which draw on the popup color:
     * their fields and buttons must not vanish into it. */
    float popup = luminance(c[ImGuiCol_PopupBg]);
    ASSERT_TRUE(luminance(c[ImGuiCol_FrameBg]) - popup >= 0.02f);
    ASSERT_TRUE(luminance(c[ImGuiCol_Button]) - popup >= 0.02f);
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

/* The unit stays visible while a number is typed: it is drawn over the right end of the field, not
 * made part of the text (which ImGui shows without it while editing). */
TEST(the_unit_sits_at_the_right_end_of_the_field_centered_vertically)
{
    ImVec2 at = theme::unit_origin(ImVec2(0, 0), ImVec2(100, 24), 20.0f, 14.0f, 8.0f);
    ASSERT_TRUE(at.x == 72.0f); /* 100 - 8 - 20 */
    ASSERT_TRUE(at.y == 5.0f);  /* (24 - 14) / 2 */
    at = theme::unit_origin(ImVec2(50, 10), ImVec2(150, 33), 21.0f, 14.0f, 6.0f);
    ASSERT_TRUE(at.x == 123.0f);
    ASSERT_TRUE(at.y == 14.0f); /* floor(10 + 4.5) */
}

/* The hex field of the color picker. */
TEST(a_color_shows_as_hex_with_alpha_only_when_translucent)
{
    const float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    ASSERT_TRUE(theme::color_hex(red) == "FF0000");
    const float teal[4] = {0.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f};
    ASSERT_TRUE(theme::color_hex(teal) == "008080");
    const float half[4] = {1.0f, 1.0f, 1.0f, 128.0f / 255.0f};
    ASSERT_TRUE(theme::color_hex(half) == "FFFFFF80");
}

TEST(hex_text_parses_in_three_six_and_eight_digit_forms)
{
    float c[4] = {0, 0, 0, 0};
    ASSERT_TRUE(theme::parse_color_hex("#ff8000", c));
    ASSERT_TRUE(c[0] == 1.0f && c[2] == 0.0f && c[3] == 1.0f);
    ASSERT_TRUE(c[1] > 0.50f && c[1] < 0.51f);
    ASSERT_TRUE(theme::parse_color_hex("F80", c)); /* shorthand: F8 88 00 */
    ASSERT_TRUE(c[0] == 1.0f && c[2] == 0.0f);
    ASSERT_TRUE(c[1] > 0.53f && c[1] < 0.54f);
    ASSERT_TRUE(theme::parse_color_hex("11223380", c));
    ASSERT_TRUE(c[3] > 0.50f && c[3] < 0.51f);
}

TEST(bad_hex_text_is_refused_and_changes_nothing)
{
    float c[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    ASSERT_TRUE(!theme::parse_color_hex("", c));
    ASSERT_TRUE(!theme::parse_color_hex("12", c));
    ASSERT_TRUE(!theme::parse_color_hex("GG0000", c));
    ASSERT_TRUE(!theme::parse_color_hex("1234567", c));
    ASSERT_TRUE(c[0] == 0.25f && c[1] == 0.5f && c[2] == 0.75f && c[3] == 1.0f);
}

/* The buttons of a dialog are told apart by their width and the gap between them: a short label
 * such as "OK" still gets a button as wide as "Cancel". */
TEST(dialog_buttons_have_a_minimum_width_and_grow_with_their_label)
{
    ASSERT_TRUE(theme::dialog_button_width(20.0f) == 88.0f);
    ASSERT_TRUE(theme::dialog_button_width(56.0f) == 88.0f);  /* 56 + 32 = 88: the boundary */
    ASSERT_TRUE(theme::dialog_button_width(90.0f) == 122.0f); /* 90 + 32 */
}

TEST(ease_toward_moves_at_a_steady_rate_and_never_overshoots)
{
    ASSERT_TRUE(theme::ease_toward(0.0f, 1.0f, 0.05f, 0.1f) == 0.5f);
    ASSERT_TRUE(theme::ease_toward(0.5f, 1.0f, 0.05f, 0.1f) == 1.0f);
    ASSERT_TRUE(theme::ease_toward(0.9f, 1.0f, 1.0f, 0.1f) == 1.0f);
    ASSERT_TRUE(theme::ease_toward(1.0f, 0.0f, 0.025f, 0.1f) == 0.75f);
    ASSERT_TRUE(theme::ease_toward(0.1f, 0.0f, 1.0f, 0.1f) == 0.0f);
    ASSERT_TRUE(theme::ease_toward(0.3f, 0.3f, 1.0f, 0.1f) == 0.3f);
    ASSERT_TRUE(theme::ease_toward(0.0f, 1.0f, 0.5f, 0.0f) == 1.0f); /* no duration: jump */
}

TEST(mix_blends_two_colors_per_channel)
{
    ASSERT_TRUE(theme::mix(0x000000, 0xffffff, 0.0f) ==
                ImGui::ColorConvertFloat4ToU32(theme::rgb(0x000000)));
    ASSERT_TRUE(theme::mix(0x000000, 0xffffff, 1.0f) ==
                ImGui::ColorConvertFloat4ToU32(theme::rgb(0xffffff)));
    ASSERT_TRUE(theme::mix(0x000000, 0xffffff, 0.5f) ==
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)));
}

TEST(a_closed_chevron_points_right_and_an_open_one_points_down)
{
    ImVec2 closed[3];
    ImVec2 open[3];
    theme::chevron_points(ImVec2(100.0f, 50.0f), 10.0f, false, closed);
    theme::chevron_points(ImVec2(100.0f, 50.0f), 10.0f, true, open);
    /* ">" : the tip is the middle point, to the right of both ends, level with the center */
    ASSERT_TRUE(closed[1].x > closed[0].x && closed[1].x > closed[2].x);
    ASSERT_TRUE(closed[1].y == 50.0f && closed[0].y < 50.0f && closed[2].y > 50.0f);
    /* "v" : the tip is below both ends, in line with the center */
    ASSERT_TRUE(open[1].y > open[0].y && open[1].y > open[2].y);
    ASSERT_TRUE(open[1].x == 100.0f && open[0].x < 100.0f && open[2].x > 100.0f);
    /* both fit in the box of the given size around the center */
    for (const ImVec2 &p : closed) {
        ASSERT_TRUE(p.x >= 95.0f && p.x <= 105.0f && p.y >= 45.0f && p.y <= 55.0f);
    }
}

int main()
{
    RUN(a_closed_chevron_points_right_and_an_open_one_points_down);
    RUN(ease_toward_moves_at_a_steady_rate_and_never_overshoots);
    RUN(mix_blends_two_colors_per_channel);
    RUN(load_fonts_adds_the_embedded_font_at_the_base_size);
    RUN(apply_keeps_the_font_size_across_a_scale_change);
    RUN(text_on_a_filled_dialog_button_reaches_the_4_5_contrast_ratio);
    RUN(a_control_fill_is_visibly_lighter_than_the_panel);
    RUN(an_int_format_appends_the_unit_and_escapes_percent);
    RUN(the_unit_sits_at_the_right_end_of_the_field_centered_vertically);
    RUN(a_color_shows_as_hex_with_alpha_only_when_translucent);
    RUN(hex_text_parses_in_three_six_and_eight_digit_forms);
    RUN(bad_hex_text_is_refused_and_changes_nothing);
    RUN(dialog_buttons_have_a_minimum_width_and_grow_with_their_label);
    return TEST_SUMMARY();
}
