#include "command_table.hpp"

const Command COMMANDS[COMMAND_COUNT] = {
    {"New...", nullptr, CommandId::New, {{ImGuiKey_N, ShiftRule::NotHeld}}, false},
    {"Open...", nullptr, CommandId::Open, {{ImGuiKey_O}}, false},
    {"Save", nullptr, CommandId::Save, {{ImGuiKey_S, ShiftRule::NotHeld}}, false},
    {"Save as...", nullptr, CommandId::SaveAs, {{ImGuiKey_S, ShiftRule::Held}}, false},
    {"Export image...", nullptr, CommandId::ExportImage, {{ImGuiKey_E}}, false},
    {"Undo", nullptr, CommandId::Undo, {{ImGuiKey_Z, ShiftRule::NotHeld}}, true},
    {"Redo", nullptr, CommandId::Redo, {{ImGuiKey_Y}, {ImGuiKey_Z, ShiftRule::Held}}, true},
    {"Cut", nullptr, CommandId::Cut, {{ImGuiKey_X}}, false},
    {"Copy", nullptr, CommandId::Copy, {{ImGuiKey_C}}, false},
    {"Paste", nullptr, CommandId::Paste, {{ImGuiKey_V}}, false},
    {"Select all", nullptr, CommandId::SelectAll, {{ImGuiKey_A}}, false},
    {"Zoom in", "Ctrl++", CommandId::ZoomIn, {{ImGuiKey_Equal}, {ImGuiKey_KeypadAdd}}, true},
    {"Zoom out", nullptr, CommandId::ZoomOut, {{ImGuiKey_Minus}, {ImGuiKey_KeypadSubtract}}, true},
    {"Fit", "F, Ctrl+0", CommandId::Fit, {{ImGuiKey_0}, {ImGuiKey_Keypad0}}, false},
    {"Timeline", nullptr, CommandId::ToggleTimeline, {{ImGuiKey_T}}, false},
    {"New layer", nullptr, CommandId::NewLayer, {{ImGuiKey_N, ShiftRule::Held}}, false},
    {"Duplicate layer", nullptr, CommandId::DuplicateLayer, {{ImGuiKey_D, ShiftRule::Held}}, false},
    {"Merge down", nullptr, CommandId::MergeLayerDown, {{ImGuiKey_M, ShiftRule::Held}}, false},
    {"Move layer up", nullptr, CommandId::MoveLayerUp, {{ImGuiKey_RightBracket}}, false},
    {"Move layer down", nullptr, CommandId::MoveLayerDown, {{ImGuiKey_LeftBracket}}, false},
    {"New frame", nullptr, CommandId::NewFrame, {{ImGuiKey_F, ShiftRule::Held}}, false},
    {"Duplicate frame", nullptr, CommandId::DuplicateFrame, {{ImGuiKey_J, ShiftRule::Held}}, false},
    {"Close tab", nullptr, CommandId::CloseTab, {{ImGuiKey_W}}, false},
    {"Next tab",
     nullptr,
     CommandId::NextTab,
     {{ImGuiKey_Tab, ShiftRule::NotHeld}, {ImGuiKey_PageDown}},
     false},
    {"Previous tab",
     nullptr,
     CommandId::PreviousTab,
     {{ImGuiKey_Tab, ShiftRule::Held}, {ImGuiKey_PageUp}},
     false},
};

namespace
{

std::string keyLabel(ImGuiKey key)
{
    if (key >= ImGuiKey_A && key <= ImGuiKey_Z) {
        char letter = static_cast<char>('A' + (key - ImGuiKey_A));
        return {letter};
    }
    if (key >= ImGuiKey_0 && key <= ImGuiKey_9) {
        char digit = static_cast<char>('0' + (key - ImGuiKey_0));
        return {digit};
    }
    switch (key) {
    case ImGuiKey_Equal:
        return "=";
    case ImGuiKey_Minus:
        return "-";
    case ImGuiKey_LeftBracket:
        return "[";
    case ImGuiKey_RightBracket:
        return "]";
    case ImGuiKey_Tab:
        return "Tab";
    default:
        return "?";
    }
}

bool shiftMatches(ShiftRule rule, bool shift)
{
    return rule == ShiftRule::Any || (rule == ShiftRule::Held) == shift;
}

} // namespace

std::string command_hint(const Command &command)
{
    if (command.hint) {
        return command.hint;
    }
    const Shortcut &first = command.shortcuts[0];
    return std::string("Ctrl+") + (first.shift == ShiftRule::Held ? "Shift+" : "") +
           keyLabel(first.key);
}

const Command *find_command(bool shift, const KeyPressed &pressed,
                            const std::function<bool(CommandId)> &enabled)
{
    for (const Command &command : COMMANDS) {
        for (const Shortcut &shortcut : command.shortcuts) {
            if (shortcut.key != ImGuiKey_None && shiftMatches(shortcut.shift, shift) &&
                pressed(shortcut.key, command.repeat) && enabled(command.id)) {
                return &command;
            }
        }
    }
    return nullptr;
}
