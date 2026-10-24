/* A greyed-out menu item says why when it is hovered (DESIGN.md, interaction principle 2: no dead
 * ends), and the commands that depend on the document's state carry the text to say it. */
#include <cstring>

#include "command_table.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "minitest.hpp"
#include "theme.hpp"
#include "ui_harness.hpp"

using test_ui::Harness;

namespace
{

constexpr int HOVER_FRAMES = 60; /* a second: longer than the tooltip delay */

/* A window with one menu item, enabled or not, followed by theme::why_disabled. Hovers the item for
 * a second and says whether a tooltip shows and what the mouse cursor is. */
struct Hover {
    bool tooltip = false;
    bool notAllowed = false;
    bool clicked = false;
};

Hover hover_item(bool enabled, const char *reason)
{
    Harness h;
    ImVec2 center(0.0f, 0.0f);
    Hover result;
    h.body = [&] {
        ImGui::SetNextWindowPos(ImVec2(300.0f, 200.0f));
        ImGui::SetNextWindowSize(ImVec2(240.0f, 80.0f));
        ImGui::Begin("items", nullptr, ImGuiWindowFlags_NoSavedSettings);
        result.clicked = ImGui::MenuItem("Greyed", nullptr, false, enabled) || result.clicked;
        theme::why_disabled(enabled, reason);
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        center = ImVec2((min.x + max.x) / 2.0f, (min.y + max.y) / 2.0f);
        ImGui::End();
    };
    h.frame(5.0f, 5.0f, false);
    for (int i = 0; i < HOVER_FRAMES; i++) {
        h.frame(center.x, center.y, false);
    }
    const ImGuiWindow *tip = ImGui::FindWindowByName("##Tooltip_00");
    result.tooltip = tip != nullptr && tip->WasActive;
    result.notAllowed = ImGui::GetMouseCursor() == ImGuiMouseCursor_NotAllowed;
    return result;
}

} // namespace

TEST(a_disabled_item_says_why_when_hovered)
{
    Hover hover = hover_item(false, "Nothing to undo");
    ASSERT_TRUE(hover.tooltip);
    ASSERT_TRUE(hover.notAllowed);
}

TEST(an_enabled_item_shows_no_reason)
{
    Hover hover = hover_item(true, "Nothing to undo");
    ASSERT_TRUE(!hover.tooltip);
    ASSERT_TRUE(!hover.notAllowed);
}

TEST(a_disabled_item_without_a_reason_still_gets_the_cursor_but_no_tooltip)
{
    Hover hover = hover_item(false, nullptr);
    ASSERT_TRUE(!hover.tooltip);
    ASSERT_TRUE(hover.notAllowed);
}

TEST(the_commands_that_depend_on_the_document_say_why_they_are_unavailable)
{
    const CommandId dependent[] = {
        CommandId::Undo,          CommandId::Redo,           CommandId::Cut,
        CommandId::Copy,          CommandId::MergeLayerDown, CommandId::MoveLayerUp,
        CommandId::MoveLayerDown, CommandId::NextTab,        CommandId::PreviousTab};
    for (CommandId id : dependent) {
        const char *reason = command_of(id).unavailable;
        ASSERT_TRUE(reason != nullptr && std::strlen(reason) > 0);
    }
    /* a command that is always available has nothing to explain */
    ASSERT_TRUE(command_of(CommandId::Save).unavailable == nullptr);
    ASSERT_TRUE(command_of(CommandId::ZoomIn).unavailable == nullptr);
}

int main()
{
    RUN(a_disabled_item_says_why_when_hovered);
    RUN(an_enabled_item_shows_no_reason);
    RUN(a_disabled_item_without_a_reason_still_gets_the_cursor_but_no_tooltip);
    RUN(the_commands_that_depend_on_the_document_say_why_they_are_unavailable);
    return TEST_SUMMARY();
}
