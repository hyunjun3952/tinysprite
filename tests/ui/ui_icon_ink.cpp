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

/* The tool and shape icons (and the palette's Replace) are drawn as strokes on one 20-unit grid,
 * so their size and weight do not depend on a font and follow any UI scale smoothly. */
static bool path_icon(icons::Icon icon)
{
    return icons::path(icon) != nullptr;
}

TEST(only_the_tool_shape_and_replace_icons_are_paths)
{
    int paths = 0;
    for (int i = 0; i < static_cast<int>(icons::Icon::Count); i++) {
        paths += path_icon(static_cast<icons::Icon>(i)) ? 1 : 0;
    }
    ASSERT_EQ_INT(18, paths);
    ASSERT_TRUE(path_icon(icons::Icon::Pencil));
    ASSERT_TRUE(path_icon(icons::Icon::Slice));
    ASSERT_TRUE(path_icon(icons::Icon::Diamond));
    ASSERT_TRUE(path_icon(icons::Icon::Replace));
    ASSERT_TRUE(!path_icon(icons::Icon::Eye));
    ASSERT_TRUE(!path_icon(icons::Icon::Folder));
    ASSERT_TRUE(icons::is_drawn(icons::Icon::Pencil));
}

/* Every path stays inside its grid and its ink has about the 15-unit length of the glyph icons
 * (INK_TARGET): from 13 (the line, a thin diagonal) to 18 (the lasso and its tail). */
TEST(every_path_fits_its_grid_with_about_the_same_ink_length)
{
    for (int i = 0; i < static_cast<int>(icons::Icon::Count); i++) {
        const icons::Path *path = icons::path(static_cast<icons::Icon>(i));
        if (path == nullptr) {
            continue;
        }
        const icons::InkBox box = icons::path_ink(*path);
        ASSERT_TRUE(box.x0 >= 0.0f && box.y0 >= 0.0f);
        ASSERT_TRUE(box.x1 <= icons::PATH_GRID && box.y1 <= icons::PATH_GRID);
        const float longest = std::max(box.x1 - box.x0, box.y1 - box.y0);
        ASSERT_TRUE(longest >= 13.0f && longest <= 18.0f);
    }
}

/* The closed shapes are outlines (no filled part), all of the one stroke weight. */
TEST(the_closed_shapes_are_outlines)
{
    using icons::Icon;
    const Icon shapes[] = {Icon::Rect,    Icon::Ellipse, Icon::Triangle, Icon::Diamond,
                           Icon::Hexagon, Icon::Star,    Icon::Cross};
    for (Icon icon : shapes) {
        const icons::Path *path = icons::path(icon);
        ASSERT_TRUE(path != nullptr);
        for (int s = 0; s < path->count; s++) {
            ASSERT_TRUE(path->strokes[s].kind != icons::StrokeKind::Fill);
        }
    }
}

/* One stroke weight for the whole set, at any scale (PATH_STROKE says why it is 2). */
TEST(the_stroke_follows_the_ui_scale)
{
    ASSERT_TRUE(icons::path_stroke(1.0f) == icons::PATH_STROKE);
    ASSERT_TRUE(icons::path_stroke(1.25f) == 1.25f * icons::PATH_STROKE);
    ASSERT_TRUE(icons::path_stroke(2.0f) == 2.0f * icons::PATH_STROKE);
}

/* What is really drawn grows with the scale, also between whole scales (the bitmaps jumped from
 * 16 to 32 pixels between 1.49 and 1.5): the drawn geometry of the pencil is about 15 px times
 * the scale at 1, 1.25, 1.5, 2 and 3. */
TEST(a_drawn_path_icon_grows_smoothly_with_the_scale)
{
    for (float scale : {1.0f, 1.25f, 1.5f, 2.0f, 3.0f}) {
        test_ui::Harness h;
        theme::setScale(scale);
        ImVec2 lo(1e9f, 1e9f);
        ImVec2 hi(-1e9f, -1e9f);
        h.body = [&] {
            ImGui::SetNextWindowPos(ImVec2(100.0f, 100.0f));
            ImGui::SetNextWindowSize(ImVec2(200.0f, 200.0f));
            ImGui::Begin("##icon_rig", nullptr, ImGuiWindowFlags_NoDecoration);
            ImDrawList *list = ImGui::GetWindowDrawList();
            const int first = list->VtxBuffer.Size;
            icons::draw(list, icons::Icon::Rect, ImVec2(150.0f, 150.0f), ImVec2(250.0f, 250.0f),
                        IM_COL32_WHITE);
            for (int v = first; v < list->VtxBuffer.Size; v++) {
                const ImDrawVert &vert = list->VtxBuffer[v];
                if ((vert.col >> IM_COL32_A_SHIFT) == 0) {
                    continue; /* the transparent edge of the anti-aliasing fringe */
                }
                lo = ImVec2(std::min(lo.x, vert.pos.x), std::min(lo.y, vert.pos.y));
                hi = ImVec2(std::max(hi.x, vert.pos.x), std::max(hi.y, vert.pos.y));
            }
            ImGui::End();
        };
        h.frame(0.0f, 0.0f, false);
        theme::setScale(1.0f);
        const float longest = std::max(hi.x - lo.x, hi.y - lo.y) / scale;
        ASSERT_TRUE(longest >= 13.5f && longest <= 15.5f); /* the square: 13 units and a stroke */
    }
}

int main()
{
    RUN(the_fit_brings_the_longer_side_to_the_target);
    RUN(the_fit_is_limited_so_a_tiny_glyph_is_not_blown_up);
    RUN(every_glyph_icon_has_about_the_same_ink_length_at_every_scale);
    RUN(only_the_tool_shape_and_replace_icons_are_paths);
    RUN(every_path_fits_its_grid_with_about_the_same_ink_length);
    RUN(the_closed_shapes_are_outlines);
    RUN(the_stroke_follows_the_ui_scale);
    RUN(a_drawn_path_icon_grows_smoothly_with_the_scale);
    return TEST_SUMMARY();
}
