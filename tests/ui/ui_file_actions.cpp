/* The file dialog's action table: every FileAction has a row, and the rows agree with each other
 * (an action that writes has an extension, a done message and a step to run). App itself needs a
 * window, so only the static table is looked at. */
#include <cstring>

#include "app.hpp"
#include "minitest.hpp"

using Action = App::FileAction;

static const Action ALL[] = {Action::Open,          Action::SaveAs,        Action::ExportImage,
                             Action::ImportPalette, Action::ExportPalette, Action::ExportGif,
                             Action::ExportSheet};

TEST(every_action_has_a_title_a_step_and_a_toast)
{
    for (Action action : ALL) {
        const App::FileActionInfo &info = App::fileInfo(action);
        ASSERT_TRUE(info.title[0] != '\0');
        ASSERT_TRUE(info.run != nullptr);
        ASSERT_TRUE(info.done[0] != '\0');
    }
}

TEST(none_has_nothing_to_run)
{
    const App::FileActionInfo &info = App::fileInfo(Action::None);
    ASSERT_TRUE(info.run == nullptr && info.options == nullptr && !info.writes());
}

TEST(titles_are_unique_because_they_name_the_popup)
{
    for (Action a : ALL) {
        for (Action b : ALL) {
            if (a != b) {
                ASSERT_TRUE(std::strcmp(App::fileInfo(a).title, App::fileInfo(b).title) != 0);
            }
        }
    }
}

TEST(writing_actions_add_a_dotted_extension_and_reading_ones_do_not)
{
    ASSERT_TRUE(!App::fileInfo(Action::Open).writes());
    ASSERT_TRUE(!App::fileInfo(Action::ImportPalette).writes());
    ASSERT_TRUE(std::strcmp(App::fileInfo(Action::SaveAs).extension, ".pxa") == 0);
    ASSERT_TRUE(std::strcmp(App::fileInfo(Action::ExportGif).extension, ".gif") == 0);
    ASSERT_TRUE(std::strcmp(App::fileInfo(Action::ExportPalette).extension, ".gpl") == 0);
    for (Action action : {Action::SaveAs, Action::ExportImage, Action::ExportPalette,
                          Action::ExportGif, Action::ExportSheet}) {
        ASSERT_TRUE(App::fileInfo(action).writes());
        ASSERT_TRUE(App::fileInfo(action).extension[0] == '.');
    }
}

TEST(only_the_exports_with_settings_have_dialog_fields)
{
    ASSERT_TRUE(App::fileInfo(Action::ExportImage).options != nullptr);
    ASSERT_TRUE(App::fileInfo(Action::ExportGif).options != nullptr);
    ASSERT_TRUE(App::fileInfo(Action::ExportSheet).options != nullptr);
    ASSERT_TRUE(App::fileInfo(Action::SaveAs).options == nullptr);
    ASSERT_TRUE(App::fileInfo(Action::Open).options == nullptr);
}

int main()
{
    RUN(every_action_has_a_title_a_step_and_a_toast);
    RUN(none_has_nothing_to_run);
    RUN(titles_are_unique_because_they_name_the_popup);
    RUN(writing_actions_add_a_dotted_extension_and_reading_ones_do_not);
    RUN(only_the_exports_with_settings_have_dialog_fields);
    return TEST_SUMMARY();
}
