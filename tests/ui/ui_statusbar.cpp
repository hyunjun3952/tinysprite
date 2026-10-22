/* The status bar text, the toast queue and the path helpers of the file dialog. No ImGui frame is
 * needed: all three are plain functions of their inputs. */
#include <memory>
#include <string>

#include "editor.hpp"
#include "file_paths.hpp"
#include "imgui_internal.h"
#include "minitest.hpp"
#include "statusbar.hpp"
#include "ui_harness.hpp"

using test_ui::Harness;

TEST(toast_shows_until_it_expires)
{
    Toasts toasts;
    ASSERT_TRUE(toasts.current(0.0) == nullptr);
    toasts.push("Saved a.pxa", false, 10.0);
    ASSERT_TRUE(toasts.current(10.1) != nullptr);
    ASSERT_TRUE(toasts.current(10.1)->text == "Saved a.pxa");
    ASSERT_TRUE(!toasts.current(10.1)->error);
    ASSERT_TRUE(toasts.current(10.0 + Toasts::INFO_SECONDS + 0.01) == nullptr);
}

TEST(error_toast_lasts_longer_and_a_new_one_replaces_the_old)
{
    Toasts toasts;
    toasts.push("Saved", false, 0.0);
    toasts.push("Cannot write x", true, 1.0);
    ASSERT_TRUE(toasts.current(1.0)->text == "Cannot write x");
    ASSERT_TRUE(toasts.current(1.0)->error);
    ASSERT_TRUE(toasts.current(1.0 + Toasts::INFO_SECONDS + 0.5) != nullptr);
    ASSERT_TRUE(toasts.current(1.0 + Toasts::ERROR_SECONDS + 0.01) == nullptr);
}

TEST(fade_is_full_then_drops_to_zero_at_the_end)
{
    Toasts toasts;
    toasts.push("x", false, 0.0);
    ASSERT_TRUE(toasts.alpha(0.5) == 1.0f);
    float near_end = toasts.alpha(Toasts::INFO_SECONDS - 0.1);
    ASSERT_TRUE(near_end > 0.0f && near_end < 1.0f);
    ASSERT_TRUE(toasts.alpha(Toasts::INFO_SECONDS + 1.0) == 0.0f);
}

TEST(extension_is_added_only_when_missing)
{
    ASSERT_TRUE(with_default_extension("art", ".pxa") == "art.pxa");
    ASSERT_TRUE(with_default_extension("art.png", ".pxa") == "art.png");
    ASSERT_TRUE(with_default_extension("dir.v2/art", ".gif") == "dir.v2/art.gif");
    ASSERT_TRUE(with_default_extension("", ".pxa").empty());
    ASSERT_TRUE(with_default_extension("art.", ".pxa") == "art.");
}

TEST(a_required_extension_is_added_unless_the_name_already_ends_in_it)
{
    ASSERT_TRUE(with_required_extension("art", ".pxa") == "art.pxa");
    ASSERT_TRUE(with_required_extension("art.png", ".pxa") == "art.png.pxa");
    ASSERT_TRUE(with_required_extension("art.pxa", ".pxa") == "art.pxa");
    ASSERT_TRUE(with_required_extension("ART.PXA", ".pxa") == "ART.PXA");
    ASSERT_TRUE(with_required_extension("", ".pxa").empty());
}

TEST(directory_of_and_join)
{
    ASSERT_TRUE(directory_of("/a/b/c.pxa") == "/a/b");
    ASSERT_TRUE(directory_of("c.pxa").empty());
    ASSERT_TRUE(directory_of("/c.pxa") == "/");
    ASSERT_TRUE(join_path("/a/b", "c.png") == "/a/b/c.png");
    ASSERT_TRUE(join_path("/a/b/", "c.png") == "/a/b/c.png");
    ASSERT_TRUE(join_path("", "c.png") == "c.png");
}

TEST(file_name_of_is_the_last_component)
{
    ASSERT_TRUE(file_name_of("/a/b/c.pxa") == "c.pxa");
    ASSERT_TRUE(file_name_of("c.pxa") == "c.pxa");
}

TEST(status_text_names_tool_frame_zoom_and_hover)
{
    Harness h;
    std::unique_ptr<Editor> editor = Editor::create(h.renderer, 16, 8);
    std::string text = status_text(*editor);
    ASSERT_TRUE(text.find("Pencil") != std::string::npos);
    ASSERT_TRUE(text.find("Frame 1/1") != std::string::npos);
    ASSERT_TRUE(text.find("Layer 1") != std::string::npos);
    ASSERT_TRUE(text.find("Zoom") != std::string::npos);
    ASSERT_TRUE(text.find("Sel") == std::string::npos);
}

TEST(status_text_shows_the_selection_size)
{
    Harness h;
    std::unique_ptr<Editor> editor = Editor::create(h.renderer, 16, 8);
    editor->setSelection(Rect{2, 1, 5, 3});
    std::string text = status_text(*editor);
    ASSERT_TRUE(text.find("Sel 5\xC3\x97"
                          "3 px") != std::string::npos);
}

/* The status bar is a row of items that can each be hidden (and put back) from its context menu.
 * The sprite's size is not one of them: the Properties section of the Inspector shows it. */
TEST(the_sprite_size_is_not_on_the_status_bar)
{
    Harness h;
    std::unique_ptr<Editor> editor = Editor::create(h.renderer, 16, 8);
    ASSERT_TRUE(status_text(*editor).find("16\xC3\x97"
                                          "8") == std::string::npos);
}

TEST(status_items_have_names_for_the_menu)
{
    ASSERT_TRUE(std::string(status_item_name(StatusItem::Tool)) == "Tool");
    ASSERT_TRUE(std::string(status_item_name(StatusItem::Zoom)) == "Zoom");
    ASSERT_TRUE(std::string(status_item_name(StatusItem::Selection)) == "Selection");
    ASSERT_TRUE(std::string(status_item_name(StatusItem::Cell)) == "Cel under the mouse");
}

TEST(a_hidden_item_is_left_out_of_the_text)
{
    Harness h;
    std::unique_ptr<Editor> editor = Editor::create(h.renderer, 16, 8);
    unsigned items = STATUS_ALL & ~status_item_bit(StatusItem::Zoom);
    std::string text = status_text(*editor, items);
    ASSERT_TRUE(text.find("Zoom") == std::string::npos);
    ASSERT_TRUE(text.find("Pencil") != std::string::npos);
    ASSERT_TRUE(text.find("Frame 1/1") != std::string::npos);
}

TEST(only_the_chosen_items_make_the_text)
{
    Harness h;
    std::unique_ptr<Editor> editor = Editor::create(h.renderer, 16, 8);
    ASSERT_TRUE(status_text(*editor, status_item_bit(StatusItem::Tool)) == "Pencil");
    ASSERT_TRUE(status_text(*editor, 0).empty());
}

TEST(the_selection_item_is_empty_without_a_selection_and_a_hidden_one_stays_out)
{
    Harness h;
    std::unique_ptr<Editor> editor = Editor::create(h.renderer, 16, 8);
    ASSERT_TRUE(status_item_text(*editor, StatusItem::Selection).empty());
    ASSERT_TRUE(status_item_text(*editor, StatusItem::Cell).empty());
    editor->setSelection(Rect{2, 1, 5, 3});
    ASSERT_TRUE(!status_item_text(*editor, StatusItem::Selection).empty());
    unsigned items = STATUS_ALL & ~status_item_bit(StatusItem::Selection);
    ASSERT_TRUE(status_text(*editor, items).find("Sel") == std::string::npos);
}

/* A right click on the bar opens its menu, where the items are switched and the bar is hidden. */
TEST(a_right_click_on_the_status_bar_opens_its_menu)
{
    Harness h;
    std::unique_ptr<Editor> editor = Editor::create(h.renderer, 16, 8);
    h.body = [&editor] { status_bar_show(*editor, 0.0f, 660.0f, 1000.0f); };
    h.frame(900, 676, false);
    h.frame(900, 676, false);
    ASSERT_EQ_INT(0, ImGui::GetCurrentContext()->OpenPopupStack.Size);
    h.button = 1;
    h.frame(900, 676, true);
    h.frame(900, 676, false);
    h.frame(900, 676, false);
    h.button = 0;
    ASSERT_TRUE(ImGui::GetCurrentContext()->OpenPopupStack.Size > 0);
}

int main()
{
    RUN(toast_shows_until_it_expires);
    RUN(error_toast_lasts_longer_and_a_new_one_replaces_the_old);
    RUN(fade_is_full_then_drops_to_zero_at_the_end);
    RUN(extension_is_added_only_when_missing);
    RUN(directory_of_and_join);
    RUN(a_required_extension_is_added_unless_the_name_already_ends_in_it);
    RUN(file_name_of_is_the_last_component);
    RUN(status_text_names_tool_frame_zoom_and_hover);
    RUN(status_text_shows_the_selection_size);
    RUN(the_sprite_size_is_not_on_the_status_bar);
    RUN(status_items_have_names_for_the_menu);
    RUN(a_hidden_item_is_left_out_of_the_text);
    RUN(only_the_chosen_items_make_the_text);
    RUN(the_selection_item_is_empty_without_a_selection_and_a_hidden_one_stays_out);
    RUN(a_right_click_on_the_status_bar_opens_its_menu);
    return TEST_SUMMARY();
}
