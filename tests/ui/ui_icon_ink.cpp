/* Material Symbols glyphs carry different padding inside the same em box, so at one font size the
 * ink of the bucket is 17 px and the plus is 9 px. Each icon is drawn at the size that brings the
 * longer side of its ink to the same length, within limits, so the icons read as one set. */
#include <algorithm>
#include <cstring>

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

/* The tool icons are bitmaps on one 16 x 16 grid, so their size does not depend on a font. */
static bool tool_icon(icons::Icon icon)
{
    return icons::bitmap(icon) != nullptr;
}

TEST(only_the_tool_and_shape_icons_are_bitmaps)
{
    ASSERT_TRUE(tool_icon(icons::Icon::Pencil));
    ASSERT_TRUE(tool_icon(icons::Icon::Slice));
    ASSERT_TRUE(tool_icon(icons::Icon::Diamond));
    ASSERT_TRUE(!tool_icon(icons::Icon::Eye));
    ASSERT_TRUE(!tool_icon(icons::Icon::Folder));
    ASSERT_TRUE(icons::is_drawn(icons::Icon::Pencil));
}

TEST(every_bitmap_is_square_has_ink_and_stays_inside_a_14_pixel_box)
{
    int bitmaps = 0;
    for (int i = 0; i < static_cast<int>(icons::Icon::Count); i++) {
        const icons::Bitmap *bitmap = icons::bitmap(static_cast<icons::Icon>(i));
        if (bitmap == nullptr) {
            continue;
        }
        bitmaps++;
        int x0 = icons::BITMAP_SIZE;
        int y0 = icons::BITMAP_SIZE;
        int x1 = -1;
        int y1 = -1;
        for (int y = 0; y < icons::BITMAP_SIZE; y++) {
            ASSERT_EQ_INT(icons::BITMAP_SIZE, static_cast<int>(std::strlen(bitmap->rows[y])));
            for (int x = 0; x < icons::BITMAP_SIZE; x++) {
                if (bitmap->rows[y][x] == '#') {
                    x0 = std::min(x0, x);
                    y0 = std::min(y0, y);
                    x1 = std::max(x1, x);
                    y1 = std::max(y1, y);
                }
            }
        }
        ASSERT_TRUE(x1 >= 0);
        ASSERT_TRUE(std::max(x1 - x0, y1 - y0) + 1 >= 11);
        ASSERT_TRUE(std::max(x1 - x0, y1 - y0) + 1 <= 14);
    }
    ASSERT_EQ_INT(15, bitmaps);
}

/* The closed shapes are outlines of one weight: an empty middle (the star was a solid fill beside
 * outlines) and about the same amount of ink (the diagonal ones had a stroke of 1.4 pixels where
 * the square and the circle have 2, so they looked thin). */
TEST(the_closed_shapes_are_outlines_of_one_weight)
{
    using icons::Icon;
    const Icon shapes[] = {Icon::Rect,    Icon::Ellipse, Icon::Triangle, Icon::Diamond,
                           Icon::Hexagon, Icon::Star,    Icon::Cross};
    for (Icon icon : shapes) {
        const icons::Bitmap *bitmap = icons::bitmap(icon);
        ASSERT_TRUE(bitmap != nullptr);
        int ink = 0;
        for (const char *row : bitmap->rows) {
            ink += static_cast<int>(std::count(row, row + std::strlen(row), '#'));
        }
        ASSERT_TRUE(bitmap->rows[8][8] == '.'); /* the middle is empty: an outline, not a fill */
        ASSERT_TRUE(ink >= 58 && ink <= 90);
    }
}

TEST(a_bitmap_cell_is_a_whole_number_of_pixels_and_at_least_one)
{
    ASSERT_TRUE(icons::bitmap_cell(1.0f) == 1.0f);
    ASSERT_TRUE(icons::bitmap_cell(1.5f) == 2.0f);
    ASSERT_TRUE(icons::bitmap_cell(2.0f) == 2.0f);
    ASSERT_TRUE(icons::bitmap_cell(0.5f) == 1.0f);
}

int main()
{
    RUN(the_fit_brings_the_longer_side_to_the_target);
    RUN(the_fit_is_limited_so_a_tiny_glyph_is_not_blown_up);
    RUN(every_glyph_icon_has_about_the_same_ink_length_at_every_scale);
    RUN(only_the_tool_and_shape_icons_are_bitmaps);
    RUN(every_bitmap_is_square_has_ink_and_stays_inside_a_14_pixel_box);
    RUN(the_closed_shapes_are_outlines_of_one_weight);
    RUN(a_bitmap_cell_is_a_whole_number_of_pixels_and_at_least_one);
    return TEST_SUMMARY();
}
