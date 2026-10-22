/* The UI font is embedded (a subset of Inter, SIL OFL), so the text looks the same on every
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
    ASSERT_TRUE(std::strstr(io.Fonts->Sources[0].Name, "Inter") != nullptr);
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

int main()
{
    RUN(load_fonts_adds_the_embedded_font_at_the_base_size);
    RUN(apply_keeps_the_font_size_across_a_scale_change);
    return TEST_SUMMARY();
}
