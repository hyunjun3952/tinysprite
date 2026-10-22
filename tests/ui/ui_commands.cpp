/* The command table behind the menus and the Ctrl shortcuts: one place for each command's label,
 * keys and hint. No window or ImGui context is needed. */
#include <algorithm>
#include <string>

#include "commands.hpp"
#include "minitest.hpp"

static const Command &command(CommandId id)
{
    for (const Command &c : COMMANDS) {
        if (c.id == id) {
            return c;
        }
    }
    static const Command none = {};
    return none;
}

/* The command a frame with exactly these keys pressed would run; every command is enabled. */
static const Command *run(bool shift, std::initializer_list<ImGuiKey> keys)
{
    return find_command(
        shift,
        [&](ImGuiKey key, bool) {
            return std::any_of(keys.begin(), keys.end(), [key](ImGuiKey k) { return k == key; });
        },
        [](CommandId) { return true; });
}

TEST(every_command_id_has_one_row)
{
    int rows = 0;
    for (int i = 0; i < COMMAND_COUNT; i++) {
        int found = 0;
        for (const Command &c : COMMANDS) {
            if (static_cast<int>(c.id) == i) {
                found++;
            }
        }
        ASSERT_EQ_INT(1, found);
        rows++;
    }
    ASSERT_EQ_INT(COMMAND_COUNT, rows);
    ASSERT_EQ_INT(COMMAND_COUNT, static_cast<int>(sizeof(COMMANDS) / sizeof(COMMANDS[0])));
}

TEST(rows_are_in_id_order_so_command_of_can_index)
{
    for (int i = 0; i < COMMAND_COUNT; i++) {
        ASSERT_EQ_INT(i, static_cast<int>(COMMANDS[i].id));
        ASSERT_TRUE(&command_of(static_cast<CommandId>(i)) == &COMMANDS[i]);
    }
}

TEST(no_two_commands_claim_the_same_keys)
{
    for (const Command &a : COMMANDS) {
        for (const Command &b : COMMANDS) {
            if (&a == &b) {
                continue;
            }
            for (const Shortcut &x : a.shortcuts) {
                for (const Shortcut &y : b.shortcuts) {
                    if (x.key == ImGuiKey_None || x.key != y.key) {
                        continue;
                    }
                    /* the same key is fine only if the Shift rules cannot both hold */
                    bool apart = (x.shift == ShiftRule::Held && y.shift == ShiftRule::NotHeld) ||
                                 (x.shift == ShiftRule::NotHeld && y.shift == ShiftRule::Held);
                    ASSERT_TRUE(apart);
                }
            }
        }
    }
}

TEST(the_hint_is_derived_from_the_first_shortcut)
{
    ASSERT_TRUE(command_hint(command(CommandId::New)) == "Ctrl+N");
    ASSERT_TRUE(command_hint(command(CommandId::Undo)) == "Ctrl+Z");
    ASSERT_TRUE(command_hint(command(CommandId::SaveAs)) == "Ctrl+Shift+S");
    ASSERT_TRUE(command_hint(command(CommandId::Redo)) == "Ctrl+Y");
}

/* The layer actions have keys now that the panel has no buttons for them. */
TEST(the_layer_commands_have_their_own_chords)
{
    ASSERT_TRUE(command_hint(command(CommandId::NewLayer)) == "Ctrl+Shift+N");
    ASSERT_TRUE(command_hint(command(CommandId::DuplicateLayer)) == "Ctrl+Shift+D");
    ASSERT_TRUE(command_hint(command(CommandId::MergeLayerDown)) == "Ctrl+Shift+M");
    ASSERT_TRUE(command_hint(command(CommandId::MoveLayerUp)) == "Ctrl+]");
    ASSERT_TRUE(command_hint(command(CommandId::MoveLayerDown)) == "Ctrl+[");
    /* Shift tells a new layer from a new document, as it tells Save from Save as */
    ASSERT_TRUE(run(false, {ImGuiKey_N})->id == CommandId::New);
    ASSERT_TRUE(run(true, {ImGuiKey_N})->id == CommandId::NewLayer);
    ASSERT_TRUE(run(true, {ImGuiKey_D})->id == CommandId::DuplicateLayer);
    ASSERT_TRUE(run(false, {ImGuiKey_RightBracket})->id == CommandId::MoveLayerUp);
    ASSERT_TRUE(run(false, {ImGuiKey_LeftBracket})->id == CommandId::MoveLayerDown);
}

TEST(the_frame_commands_have_their_own_chords)
{
    ASSERT_TRUE(command_hint(command(CommandId::NewFrame)) == "Ctrl+Shift+F");
    ASSERT_TRUE(command_hint(command(CommandId::DuplicateFrame)) == "Ctrl+Shift+J");
    ASSERT_TRUE(run(true, {ImGuiKey_F})->id == CommandId::NewFrame);
    ASSERT_TRUE(run(true, {ImGuiKey_J})->id == CommandId::DuplicateFrame);
}

TEST(the_tab_commands_have_their_own_chords)
{
    ASSERT_TRUE(command_hint(command(CommandId::CloseTab)) == "Ctrl+W");
    ASSERT_TRUE(command_hint(command(CommandId::NextTab)) == "Ctrl+Tab");
    ASSERT_TRUE(command_hint(command(CommandId::PreviousTab)) == "Ctrl+Shift+Tab");
    ASSERT_TRUE(run(false, {ImGuiKey_W})->id == CommandId::CloseTab);
    ASSERT_TRUE(run(false, {ImGuiKey_Tab})->id == CommandId::NextTab);
    ASSERT_TRUE(run(true, {ImGuiKey_Tab})->id == CommandId::PreviousTab);
    ASSERT_TRUE(run(false, {ImGuiKey_PageDown})->id == CommandId::NextTab);
    ASSERT_TRUE(run(false, {ImGuiKey_PageUp})->id == CommandId::PreviousTab);
}

TEST(a_hint_can_be_spelled_out)
{
    ASSERT_TRUE(command_hint(command(CommandId::ZoomIn)) == "Ctrl++");
    ASSERT_TRUE(command_hint(command(CommandId::Fit)) == "F, Ctrl+0");
}

TEST(shift_tells_save_from_save_as_and_undo_from_redo)
{
    ASSERT_TRUE(run(false, {ImGuiKey_S})->id == CommandId::Save);
    ASSERT_TRUE(run(true, {ImGuiKey_S})->id == CommandId::SaveAs);
    ASSERT_TRUE(run(false, {ImGuiKey_Z})->id == CommandId::Undo);
    ASSERT_TRUE(run(true, {ImGuiKey_Z})->id == CommandId::Redo);
    ASSERT_TRUE(run(false, {ImGuiKey_Y})->id == CommandId::Redo);
    ASSERT_TRUE(run(true, {ImGuiKey_Y})->id == CommandId::Redo);
}

TEST(shift_is_ignored_where_a_command_does_not_care)
{
    ASSERT_TRUE(run(true, {ImGuiKey_O})->id == CommandId::Open);
    ASSERT_TRUE(run(true, {ImGuiKey_Equal})->id == CommandId::ZoomIn);
    ASSERT_TRUE(run(false, {ImGuiKey_KeypadAdd})->id == CommandId::ZoomIn);
    ASSERT_TRUE(run(false, {ImGuiKey_Keypad0})->id == CommandId::Fit);
}

TEST(ctrl_t_toggles_the_timeline)
{
    const Command &toggle = command(CommandId::ToggleTimeline);
    ASSERT_TRUE(std::string(toggle.label) == "Timeline");
    ASSERT_TRUE(command_hint(toggle) == "Ctrl+T");
    ASSERT_TRUE(run(false, {ImGuiKey_T}) == &toggle);
}

TEST(no_key_runs_nothing)
{
    ASSERT_TRUE(run(false, {}) == nullptr);
    ASSERT_TRUE(run(false, {ImGuiKey_Q}) == nullptr);
}

TEST(a_disabled_command_does_not_run)
{
    const Command *c = find_command(
        false, [](ImGuiKey key, bool) { return key == ImGuiKey_Z; },
        [](CommandId id) { return id != CommandId::Undo; });
    ASSERT_TRUE(c == nullptr);
}

TEST(only_undo_redo_and_zoom_repeat_while_held)
{
    for (const Command &c : COMMANDS) {
        bool repeats = c.id == CommandId::Undo || c.id == CommandId::Redo ||
                       c.id == CommandId::ZoomIn || c.id == CommandId::ZoomOut;
        ASSERT_TRUE(c.repeat == repeats);
    }
}

int main()
{
    RUN(every_command_id_has_one_row);
    RUN(rows_are_in_id_order_so_command_of_can_index);
    RUN(no_two_commands_claim_the_same_keys);
    RUN(the_hint_is_derived_from_the_first_shortcut);
    RUN(the_layer_commands_have_their_own_chords);
    RUN(the_frame_commands_have_their_own_chords);
    RUN(the_tab_commands_have_their_own_chords);
    RUN(a_hint_can_be_spelled_out);
    RUN(shift_tells_save_from_save_as_and_undo_from_redo);
    RUN(shift_is_ignored_where_a_command_does_not_care);
    RUN(ctrl_t_toggles_the_timeline);
    RUN(no_key_runs_nothing);
    RUN(a_disabled_command_does_not_run);
    RUN(only_undo_redo_and_zoom_repeat_while_held);
    return TEST_SUMMARY();
}
