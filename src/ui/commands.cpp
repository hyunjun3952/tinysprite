#include "commands.h"

const Command COMMANDS[COMMAND_COUNT] = {
    {"New...", nullptr, CommandId::New, {{ImGuiKey_N}}, false},
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
