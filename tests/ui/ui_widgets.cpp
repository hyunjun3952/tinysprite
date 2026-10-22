/* The slider and the combo of the theme are drawn over invisible ImGui widgets: the input must
 * still work, and the field must show the value as a fill. */
#include "imgui.h"
#include "minitest.hpp"
#include "theme.hpp"
#include "ui_harness.hpp"

namespace
{

constexpr float FIELD_WIDTH = 200.0f;

/* A window with one slider 0..100 whose rectangle is stored in `min` and `max`. */
struct SliderRig {
    test_ui::Harness h;
    int value = 0;
    ImVec2 min;
    ImVec2 max;

    SliderRig()
    {
        h.body = [this] {
            ImGui::SetNextWindowPos(ImVec2(100.0f, 100.0f));
            ImGui::SetNextWindowSize(ImVec2(260.0f, 80.0f));
            ImGui::Begin("##rig", nullptr, ImGuiWindowFlags_NoDecoration);
            ImGui::SetNextItemWidth(FIELD_WIDTH);
            theme::slider_int("##slider", &value, 0, 100, "%");
            min = ImGui::GetItemRectMin();
            max = ImGui::GetItemRectMax();
            ImGui::End();
        };
        h.frame(0.0f, 0.0f, false);
    }
    float centerY() const
    {
        return (min.y + max.y) / 2.0f;
    }
};

} // namespace

TEST(dragging_a_slider_sets_the_value_by_the_mouse_position)
{
    SliderRig rig;
    float y = rig.centerY();
    rig.h.drag(rig.min.x + 2.0f, y, rig.min.x + FIELD_WIDTH / 2.0f, y);
    ASSERT_TRUE(rig.value >= 40 && rig.value <= 60);
    rig.h.drag(rig.min.x + 2.0f, y, rig.max.x + 100.0f, y); /* past the end */
    ASSERT_EQ_INT(100, rig.value);
}

TEST(a_slider_is_filled_up_to_its_value)
{
    SliderRig rig;
    rig.value = 50;
    rig.h.frame(0.0f, 0.0f, false);
    int y = static_cast<int>(rig.centerY()) + 6; /* under the digits */
    ASSERT_TRUE(rig.h.pixel(static_cast<int>(rig.min.x) + 20, y) == theme::SLIDER_FILL);
    ASSERT_TRUE(rig.h.pixel(static_cast<int>(rig.max.x) - 20, y) == theme::CANVAS_SOFT);
}

TEST(an_empty_slider_has_no_fill)
{
    SliderRig rig;
    rig.h.frame(0.0f, 0.0f, false);
    int y = static_cast<int>(rig.centerY()) + 6;
    ASSERT_TRUE(rig.h.pixel(static_cast<int>(rig.min.x) + 20, y) == theme::CANVAS_SOFT);
}

TEST(a_modal_fades_in_when_it_opens)
{
    test_ui::Harness h;
    bool opened = false;
    int frames = 0;
    int shown = 0;
    float firstAlpha = -1.0f;
    float lastAlpha = -1.0f;
    h.body = [&] {
        ImGui::GetIO().DeltaTime = 0.05f; /* the frames of a test are far shorter than real ones */
        /* like the app's dialogs: asked every frame, opened a few frames in */
        if (!opened && ++frames == 3) {
            ImGui::OpenPopup("Fade test");
            opened = true;
        }
        if (theme::begin_modal("Fade test")) {
            lastAlpha = ImGui::GetStyle().Alpha;
            firstAlpha = shown++ == 0 ? lastAlpha : firstAlpha;
            ImGui::TextUnformatted("hello");
            theme::end_modal();
        }
    };
    for (int i = 0; i < 400 && lastAlpha < 1.0f; i++) {
        h.frame(0.0f, 0.0f, false);
    }
    ASSERT_TRUE(shown > 1);
    ASSERT_TRUE(firstAlpha > 0.0f && firstAlpha < 1.0f);
    ASSERT_TRUE(lastAlpha == 1.0f);
    ASSERT_TRUE(ImGui::GetStyle().Alpha == 1.0f); /* nothing leaks out of the dialog */
}

/* A menu bar with one menu, "File", whose title is at `title` once drawn. */
struct MenuRig {
    test_ui::Harness h;
    ImVec2 title;

    MenuRig()
    {
        h.body = [this] {
            ImGui::GetIO().DeltaTime =
                0.1f; /* the frames of a test are far shorter than real ones */
            if (ImGui::BeginMainMenuBar()) {
                if (theme::begin_menu("File")) {
                    ImGui::MenuItem("Open");
                    ImGui::EndMenu();
                }
                if (title.x == 0.0f) {
                    title = ImVec2(20.0f, ImGui::GetWindowHeight() / 2.0f);
                }
                ImGui::EndMainMenuBar();
            }
        };
        h.frame(0.0f, 300.0f, false);
    }
    bool open() const
    {
        return ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    }
    void frames(float x, float y, int count)
    {
        for (int i = 0; i < count; i++) {
            h.frame(x, y, false);
        }
    }
};

TEST(a_menu_opens_on_hover_and_closes_when_the_mouse_leaves)
{
    MenuRig rig;
    rig.frames(rig.title.x, rig.title.y, 3);
    ASSERT_TRUE(rig.open());
    rig.frames(rig.title.x, 15.0f + 0.0f, 3); /* still on the bar: stays */
    ASSERT_TRUE(rig.open());
    rig.frames(700.0f, 500.0f, 6); /* far away from the bar and the menu */
    ASSERT_TRUE(!rig.open());
}

TEST(a_menu_stays_open_while_the_mouse_is_over_it)
{
    MenuRig rig;
    rig.frames(rig.title.x, rig.title.y, 3);
    ASSERT_TRUE(rig.open());
    /* the first item sits just under the bar */
    rig.frames(rig.title.x + 10.0f, ImGui::GetFrameHeight() + 14.0f, 8);
    ASSERT_TRUE(rig.open());
}

int main()
{
    RUN(dragging_a_slider_sets_the_value_by_the_mouse_position);
    RUN(a_slider_is_filled_up_to_its_value);
    RUN(an_empty_slider_has_no_fill);
    RUN(a_modal_fades_in_when_it_opens);
    RUN(a_menu_opens_on_hover_and_closes_when_the_mouse_leaves);
    RUN(a_menu_stays_open_while_the_mouse_is_over_it);
    return TEST_SUMMARY();
}
