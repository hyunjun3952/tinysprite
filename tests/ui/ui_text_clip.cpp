/* A name wider than its cell is cut with "..." so the reader sees it is cut; one that fits stays
 * as it is. */
#include <string>

#include "imgui.h"
#include "minitest.hpp"
#include "theme.hpp"
#include "ui_harness.hpp"

namespace
{

/* The result of theme::ellipsize(text, width) measured inside a frame. */
std::string clipped(const std::string &text, float width, float *drawn = nullptr)
{
    test_ui::Harness h;
    std::string result;
    h.body = [&] {
        ImGui::Begin("##rig");
        result = theme::ellipsize(text, width);
        if (drawn) {
            *drawn = ImGui::CalcTextSize(result.c_str()).x;
        }
        ImGui::End();
    };
    h.frame(0.0f, 0.0f, false);
    return result;
}

} // namespace

TEST(a_name_that_fits_is_left_alone)
{
    ASSERT_TRUE(clipped("Layer 1", 500.0f) == "Layer 1");
    ASSERT_TRUE(clipped("", 10.0f).empty());
}

TEST(a_long_name_is_cut_to_the_width_and_ends_in_dots)
{
    float drawn = 0.0f;
    std::string text(80, 'W');
    std::string result = clipped(text, 100.0f, &drawn);
    ASSERT_TRUE(result.size() < text.size());
    ASSERT_TRUE(result.size() >= 3 && result.substr(result.size() - 3) == "...");
    ASSERT_TRUE(drawn <= 100.0f);
}

TEST(a_cut_never_splits_a_utf8_character)
{
    std::string text;
    for (int i = 0; i < 30; i++) {
        text += "\xc3\xbc"; /* u with diaeresis, two bytes */
    }
    std::string result = clipped(text, 60.0f);
    std::string head = result.substr(0, result.size() - 3);
    ASSERT_TRUE(head.size() % 2 == 0);
}

TEST(a_width_too_small_for_the_dots_gives_the_dots)
{
    ASSERT_TRUE(clipped("Layer 1", 1.0f) == "...");
}

int main()
{
    RUN(a_name_that_fits_is_left_alone);
    RUN(a_long_name_is_cut_to_the_width_and_ends_in_dots);
    RUN(a_cut_never_splits_a_utf8_character);
    RUN(a_width_too_small_for_the_dots_gives_the_dots);
    return TEST_SUMMARY();
}
